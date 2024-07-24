/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "NBAIO_Tee"
//#define LOG_NDEBUG 0

#include <utils/Log.h>

#include <deque>
#include <dirent.h>
#include <future>
#include <list>
#include <vector>

#include <audio_utils/format.h>
#include <audio_utils/sndfile.h>
#include <media/nbaio/PipeReader.h>

#include "Configuration.h"
#include "NBAIO_Tee.h"

// Enabled with TEE_SINK in Configuration.h
#ifdef TEE_SINK

namespace android {

/*
 Tee filenames generated as follows:

 "aftee_Date_ThreadId_C_reason.wav" RecordThread
 "aftee_Date_ThreadId_M_reason.wav" MixerThread (Normal)
 "aftee_Date_ThreadId_F_reason.wav" MixerThread (Fast)
 "aftee_Date_ThreadId_D_reason.raw" DirectOutputThread (SpdifStreamOut)
 "aftee_Date_ThreadId_TrackId_R_reason.wav" RecordTrack
 "aftee_Date_ThreadId_TrackId_TrackName_T_reason.wav" PlaybackTrack

 where Date = YYYYmmdd_HHMMSS_MSEC

 where Reason = [ DTOR | DUMP | REMOVE ]

 Examples:
  aftee_20180424_153811_038_13_57_2_T_REMOVE.wav
  aftee_20180424_153811_218_13_57_2_T_REMOVE.wav
  aftee_20180424_153811_378_13_57_2_T_REMOVE.wav
  aftee_20180424_153825_147_62_C_DUMP.wav
  aftee_20180424_153825_148_62_59_R_DUMP.wav
  aftee_20180424_153825_149_13_F_DUMP.wav
  aftee_20180424_153842_125_62_59_R_REMOVE.wav
  aftee_20180424_153842_168_62_C_DTOR.wav
*/

static constexpr char DEFAULT_PREFIX[] = "aftee_";
static constexpr char DEFAULT_DIRECTORY[] = "/data/misc/audioserver";
static constexpr size_t DEFAULT_THREADPOOL_SIZE = 8;

/** AudioFileHandler manages temporary audio wav files with a least recently created
    retention policy.

    The temporary filenames are systematically generated. A common filename prefix,
    storage directory, and concurrency pool are passed in on creating the object.

    Temporary files are created by "create", which returns a filename generated by

    prefix + 14 char date + suffix

    TODO Move to audio_utils.
    TODO Avoid pointing two AudioFileHandlers to the same directory and prefix
    as we don't have a prefix specific lock file. */

class AudioFileHandler {
public:

    AudioFileHandler(const std::string &prefix, const std::string &directory, size_t pool)
        : mThreadPool(pool)
        , mPrefix(prefix)
    {
        (void)setDirectory(directory);
    }

    /** returns filename of created audio file, else empty string on failure. */
    std::string create(
            const std::function<ssize_t /* frames_read */
                        (void * /* buffer */, size_t /* size_in_frames */)>& reader,
            uint32_t sampleRate,
            uint32_t channelCount,
            audio_format_t format,
            const std::string &suffix);

private:
    /** sets the current directory. this is currently private to avoid confusion
        when changing while pending operations are occurring (it's okay, but
        weakly synchronized). */
    status_t setDirectory(const std::string &directory);

    /** cleans current directory and returns the directory name done. */
    status_t clean(std::string *dir = nullptr);

    /** creates an audio file from a reader functor passed in. */
    status_t createInternal(
            const std::function<ssize_t /* frames_read */
                        (void * /* buffer */, size_t /* size_in_frames */)>& reader,
            uint32_t sampleRate,
            uint32_t channelCount,
            audio_format_t format,
            const std::string &filename);

    static bool isDirectoryValid(const std::string &directory) {
        return directory.size() > 0 && directory[0] == '/';
    }

    std::string generateFilename(const std::string &suffix, audio_format_t format) const {
        char fileTime[sizeof("YYYYmmdd_HHMMSS_\0")];
        struct timeval tv;
        gettimeofday(&tv, nullptr /* struct timezone */);
        struct tm tm;
        localtime_r(&tv.tv_sec, &tm);
        LOG_ALWAYS_FATAL_IF(strftime(fileTime, sizeof(fileTime), "%Y%m%d_%H%M%S_", &tm) == 0,
            "incorrect fileTime buffer");
        char msec[4];
        (void)snprintf(msec, sizeof(msec), "%03d", (int)(tv.tv_usec / 1000));
        return mPrefix + fileTime + msec + suffix + (audio_is_linear_pcm(format) ? ".wav" : ".raw");
    }

    bool isManagedFilename(const char *name) {
        constexpr size_t FILENAME_LEN_DATE = 4 + 2 + 2 // %Y%m%d%
            + 1 + 2 + 2 + 2 // _H%M%S
            + 1 + 3; //_MSEC
        const size_t prefixLen = mPrefix.size();
        const size_t nameLen = strlen(name);

        // reject on size, prefix, and .wav
        if (nameLen < prefixLen + FILENAME_LEN_DATE + 4 /* .wav */
             || strncmp(name, mPrefix.c_str(), prefixLen) != 0
             || strcmp(name + nameLen - 4, ".wav") != 0) {
            return false;
        }

        // validate date portion
        const char *date = name + prefixLen;
        return std::all_of(date, date + 8, isdigit)
            && date[8] == '_'
            && std::all_of(date + 9, date + 15, isdigit)
            && date[15] == '_'
            && std::all_of(date + 16, date + 19, isdigit);
    }

    // yet another ThreadPool implementation.
    class ThreadPool {
    public:
        explicit ThreadPool(size_t size)
            : mThreadPoolSize(size)
        { }

        /** launches task "name" with associated function "func".
            if the threadpool is exhausted, it will launch on calling function */
        status_t launch(const std::string &name, const std::function<status_t()>& func);

    private:
        const size_t mThreadPoolSize;
        std::mutex mLock;
        std::list<std::pair<
                std::string, std::future<status_t>>> mFutures; // GUARDED_BY(mLock);
    } mThreadPool;

    static constexpr size_t FRAMES_PER_READ = 1024;
    static constexpr size_t MAX_FILES_READ = 1024;
    static constexpr size_t MAX_FILES_KEEP = 32;

    const std::string mPrefix;
    std::mutex mLock;
    std::string mDirectory;         // GUARDED_BY(mLock);
    std::deque<std::string> mFiles; // GUARDED_BY(mLock); // sorted list of files by creation time
};

/* static */
void NBAIO_Tee::NBAIO_TeeImpl::dumpTee(
        int fd, const NBAIO_SinkSource &sinkSource, const std::string &suffix)
{
    // Singleton. Constructed thread-safe on first call, never destroyed.
    static AudioFileHandler audioFileHandler(
            DEFAULT_PREFIX, DEFAULT_DIRECTORY, DEFAULT_THREADPOOL_SIZE);

    auto &source = sinkSource.second;
    if (source.get() == nullptr) {
        return;
    }

    const NBAIO_Format format = source->format();
    bool firstRead = true;
    const std::string filename = audioFileHandler.create(
            // this functor must not hold references to stack
            [firstRead, sinkSource] (void *buffer, size_t frames) mutable {
                    auto &source = sinkSource.second;
                    ssize_t actualRead = source->read(buffer, frames);
                    if (actualRead == (ssize_t)OVERRUN && firstRead) {
                        // recheck once
                        actualRead = source->read(buffer, frames);
                    }
                    firstRead = false;
                    return actualRead;
                },
            Format_sampleRate(format),
            Format_channelCount(format),
            format.mFormat,
            suffix);

    if (fd >= 0 && filename.size() > 0) {
        dprintf(fd, "tee wrote to %s\n", filename.c_str());
    }
}

/* static */
NBAIO_Tee::NBAIO_TeeImpl::NBAIO_SinkSource NBAIO_Tee::NBAIO_TeeImpl::makeSinkSource(
        const NBAIO_Format &format, size_t frames, bool *enabled)
{
    if (Format_isValid(format) && audio_has_proportional_frames(format.mFormat)) {
        Pipe *pipe = new Pipe(frames, format);
        size_t numCounterOffers = 0;
        const NBAIO_Format offers[1] = {format};
        ssize_t index = pipe->negotiate(
                offers, 1 /* numOffers */, nullptr /* counterOffers */, numCounterOffers);
        if (index != 0) {
            ALOGW("pipe failure to negotiate: %zd", index);
            goto exit;
        }
        PipeReader *pipeReader = new PipeReader(*pipe);
        numCounterOffers = 0;
        index = pipeReader->negotiate(
                offers, 1 /* numOffers */, nullptr /* counterOffers */, numCounterOffers);
        if (index != 0) {
            ALOGW("pipeReader failure to negotiate: %zd", index);
            goto exit;
        }
        if (enabled != nullptr) *enabled = true;
        return {pipe, pipeReader};
    }
exit:
    if (enabled != nullptr) *enabled = false;
    return {nullptr, nullptr};
}

std::string AudioFileHandler::create(
        const std::function<ssize_t /* frames_read */
                    (void * /* buffer */, size_t /* size_in_frames */)>& reader,
        uint32_t sampleRate,
        uint32_t channelCount,
        audio_format_t format,
        const std::string &suffix)
{
    std::string filename = generateFilename(suffix, format);

    if (mThreadPool.launch(std::string("create ") + filename,
            [=]() { return createInternal(reader, sampleRate, channelCount, format, filename); })
            == NO_ERROR) {
        return filename;
    }
    return "";
}

status_t AudioFileHandler::setDirectory(const std::string &directory)
{
    if (!isDirectoryValid(directory)) return BAD_VALUE;

    // TODO: consider using std::filesystem in C++17
    DIR *dir = opendir(directory.c_str());

    if (dir == nullptr) {
        ALOGW("%s: cannot open directory %s", __func__, directory.c_str());
        return BAD_VALUE;
    }

    size_t toRemove = 0;
    decltype(mFiles) files;

    while (files.size() < MAX_FILES_READ) {
        errno = 0;
        const struct dirent *result = readdir(dir);
        if (result == nullptr) {
            ALOGW_IF(errno != 0, "%s: readdir failure %s", __func__, strerror(errno));
            break;
        }
        // is it a managed filename?
        if (!isManagedFilename(result->d_name)) {
            continue;
        }
        files.emplace_back(result->d_name);
    }
    (void)closedir(dir);

    // OPTIMIZATION: we don't need to stat each file, the filenames names are
    // already (roughly) ordered by creation date.  we use std::deque instead
    // of std::set for faster insertion and sorting times.

    if (files.size() > MAX_FILES_KEEP) {
        // removed files can use a partition (no need to do a full sort).
        toRemove = files.size() - MAX_FILES_KEEP;
        std::nth_element(files.begin(), files.begin() + toRemove - 1, files.end());
    }

    // kept files must be sorted.
    std::sort(files.begin() + toRemove, files.end());

    {
        const std::lock_guard<std::mutex> _l(mLock);

        mDirectory = directory;
        mFiles = std::move(files);
    }

    if (toRemove > 0) { // launch a clean in background.
        (void)mThreadPool.launch(
                std::string("cleaning ") + directory, [this]() { return clean(); });
    }
    return NO_ERROR;
}

status_t AudioFileHandler::clean(std::string *directory)
{
    std::vector<std::string> filesToRemove;
    std::string dir;
    {
        const std::lock_guard<std::mutex> _l(mLock);

        if (!isDirectoryValid(mDirectory)) return NO_INIT;

        dir = mDirectory;
        if (mFiles.size() > MAX_FILES_KEEP) {
            const size_t toRemove = mFiles.size() - MAX_FILES_KEEP;

            // use move and erase to efficiently transfer std::string
            std::move(mFiles.begin(),
                    mFiles.begin() + toRemove,
                    std::back_inserter(filesToRemove));
            mFiles.erase(mFiles.begin(), mFiles.begin() + toRemove);
        }
    }

    const std::string dirp = dir + "/";
    // remove files outside of lock for better concurrency.
    for (const auto &file : filesToRemove) {
        (void)unlink((dirp + file).c_str());
    }

    // return the directory if requested.
    if (directory != nullptr) {
        *directory = dir;
    }
    return NO_ERROR;
}

status_t AudioFileHandler::ThreadPool::launch(
        const std::string &name, const std::function<status_t()>& func)
{
    if (mThreadPoolSize > 1) {
        const std::lock_guard<std::mutex> _l(mLock);
        if (mFutures.size() >= mThreadPoolSize) {
            for (auto it = mFutures.begin(); it != mFutures.end();) {
                const std::string &filename = it->first;
                const std::future<status_t> &future = it->second;
                if (!future.valid() ||
                        future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    ALOGV("%s: future %s ready", __func__, filename.c_str());
                    it = mFutures.erase(it);
                } else {
                    ALOGV("%s: future %s not ready", __func__, filename.c_str());
                    ++it;
                }
            }
        }
        if (mFutures.size() < mThreadPoolSize) {
            ALOGV("%s: deferred calling %s", __func__, name.c_str());
            mFutures.emplace_back(name, std::async(std::launch::async, func));
            return NO_ERROR;
        }
    }
    ALOGV("%s: immediate calling %s", __func__, name.c_str());
    return func();
}

status_t AudioFileHandler::createInternal(
        const std::function<ssize_t /* frames_read */
                    (void * /* buffer */, size_t /* size_in_frames */)>& reader,
        uint32_t sampleRate,
        uint32_t channelCount,
        audio_format_t format,
        const std::string &filename)
{
    // Attempt to choose the best matching file format.
    // We can choose any sf_format
    // but writeFormat must be one of 16, 32, float
    // due to sf_writef compatibility.
    int sf_format;
    audio_format_t writeFormat;
    switch (format) {
    case AUDIO_FORMAT_PCM_8_BIT:
    case AUDIO_FORMAT_PCM_16_BIT:
    case AUDIO_FORMAT_IEC61937:
        sf_format = SF_FORMAT_PCM_16;
        writeFormat = AUDIO_FORMAT_PCM_16_BIT;
        ALOGV("%s: %s using PCM_16 for format %#x", __func__, filename.c_str(), format);
        break;
    case AUDIO_FORMAT_PCM_8_24_BIT:
    case AUDIO_FORMAT_PCM_24_BIT_PACKED:
    case AUDIO_FORMAT_PCM_32_BIT:
        sf_format = SF_FORMAT_PCM_32;
        writeFormat = AUDIO_FORMAT_PCM_32_BIT;
        ALOGV("%s: %s using PCM_32 for format %#x", __func__, filename.c_str(), format);
        break;
    case AUDIO_FORMAT_PCM_FLOAT:
        sf_format = SF_FORMAT_FLOAT;
        writeFormat = AUDIO_FORMAT_PCM_FLOAT;
        ALOGV("%s: %s using PCM_FLOAT for format %#x", __func__, filename.c_str(), format);
        break;
    default:
        // TODO:
        // handle compressed formats as single byte files.
        return BAD_VALUE;
    }

    std::string directory;
    const status_t status = clean(&directory);
    if (status != NO_ERROR) return status;
    const std::string dirPrefix = directory + "/";

    const std::string path = dirPrefix + filename;

    /* const */ SF_INFO info = {
        .frames = 0,
        .samplerate = (int)sampleRate,
        .channels = (int)channelCount,
        .format = sf_format | (audio_is_linear_pcm(format) ? SF_FORMAT_WAV : 0 /* RAW */),
    };
    SNDFILE *sf = sf_open(path.c_str(), SFM_WRITE, &info);
    if (sf == nullptr) {
        return INVALID_OPERATION;
    }

    size_t total = 0;
    void *buffer = malloc(FRAMES_PER_READ * std::max(
            channelCount * audio_bytes_per_sample(writeFormat), //output framesize
            channelCount * audio_bytes_per_sample(format))); // input framesize
    if (buffer == nullptr) {
        sf_close(sf);
        return NO_MEMORY;
    }

    for (;;) {
        const ssize_t actualRead = reader(buffer, FRAMES_PER_READ);
        if (actualRead <= 0) {
            break;
        }

        // Convert input format to writeFormat as needed.
        if (format != writeFormat && audio_is_linear_pcm(format)) {
            memcpy_by_audio_format(
                    buffer, writeFormat, buffer, format, actualRead * info.channels);
        }

        ssize_t reallyWritten;
        switch (writeFormat) {
        case AUDIO_FORMAT_PCM_16_BIT:
            reallyWritten = sf_writef_short(sf, (const int16_t *)buffer, actualRead);
            break;
        case AUDIO_FORMAT_PCM_32_BIT:
            reallyWritten = sf_writef_int(sf, (const int32_t *)buffer, actualRead);
            break;
        case AUDIO_FORMAT_PCM_FLOAT:
            reallyWritten = sf_writef_float(sf, (const float *)buffer, actualRead);
            break;
        default:
            LOG_ALWAYS_FATAL("%s: %s writeFormat: %#x", __func__, filename.c_str(), writeFormat);
            break;
        }

        if (reallyWritten < 0) {
            ALOGW("%s: %s write error: %zd", __func__, filename.c_str(), reallyWritten);
            break;
        }
        total += reallyWritten;
        if (reallyWritten < actualRead) {
            ALOGW("%s: %s write short count: %zd < %zd",
                     __func__, filename.c_str(), reallyWritten, actualRead);
            break;
        }
    }
    sf_close(sf);
    free(buffer);
    if (total == 0) {
        (void)unlink(path.c_str());
        return NOT_ENOUGH_DATA;
    }

    // Success: add our name to managed files.
    {
        const std::lock_guard<std::mutex> _l(mLock);
        // weak synchronization - only update mFiles if the directory hasn't changed.
        if (mDirectory == directory) {
            mFiles.emplace_back(filename);  // add to the end to preserve sort.
        }
    }
    return NO_ERROR; // return full path
}

/* static */
NBAIO_Tee::RunningTees& NBAIO_Tee::getRunningTees() {
    [[clang::no_destroy]] static RunningTees runningTees;
    return runningTees;
}

} // namespace android

#endif // TEE_SINK
