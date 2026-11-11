#include "app/services/AnimationImport.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "core/formats/ImageFile.h"
#include "core/library/Asset.h"

namespace pixie::services {

namespace {

// Case-insensitive, with runs of digits compared as numbers.
bool NaturalLess(const std::string& left, const std::string& right) {
    size_t leftPos = 0;
    size_t rightPos = 0;
    while (leftPos < left.size() && rightPos < right.size()) {
        const bool bothDigits = std::isdigit(static_cast<unsigned char>(left[leftPos])) &&
                                std::isdigit(static_cast<unsigned char>(right[rightPos]));
        if (bothDigits) {
            size_t leftEnd = leftPos;
            size_t rightEnd = rightPos;
            while (leftEnd < left.size() && std::isdigit(static_cast<unsigned char>(left[leftEnd]))) ++leftEnd;
            while (rightEnd < right.size() && std::isdigit(static_cast<unsigned char>(right[rightEnd]))) ++rightEnd;
            std::string leftNumber = left.substr(leftPos, leftEnd - leftPos);
            std::string rightNumber = right.substr(rightPos, rightEnd - rightPos);
            leftNumber.erase(0, std::min(leftNumber.find_first_not_of('0'), leftNumber.size()));
            rightNumber.erase(0, std::min(rightNumber.find_first_not_of('0'), rightNumber.size()));
            if (leftNumber.size() != rightNumber.size()) return leftNumber.size() < rightNumber.size();
            if (leftNumber != rightNumber) return leftNumber < rightNumber;
            leftPos = leftEnd;
            rightPos = rightEnd;
            continue;
        }
        const int leftChar = std::tolower(static_cast<unsigned char>(left[leftPos]));
        const int rightChar = std::tolower(static_cast<unsigned char>(right[rightPos]));
        if (leftChar != rightChar) return leftChar < rightChar;
        ++leftPos;
        ++rightPos;
    }
    return left.size() - leftPos < right.size() - rightPos;
}

std::string Utf8FileName(const std::filesystem::path& path) {
    const std::u8string name = path.filename().u8string();
    return std::string(reinterpret_cast<const char*>(name.data()), name.size());
}

void Fail(BatchProgress& progress, const std::string& message) {
    progress.saved = 0;
    progress.failed = 1;
    progress.firstError = message;
}

} // namespace

std::vector<std::filesystem::path> ListAnimationFrames(const std::filesystem::path& folder) {
    std::vector<std::filesystem::path> frames;
    std::error_code error;
    for (std::filesystem::directory_iterator it(folder, error), end; !error && it != end; it.increment(error)) {
        std::error_code typeError;
        if (it->is_regular_file(typeError) && library::IsBaseImageFile(it->path())) frames.push_back(it->path());
    }
    std::sort(frames.begin(), frames.end(), [](const std::filesystem::path& left, const std::filesystem::path& right) {
        return NaturalLess(Utf8FileName(left), Utf8FileName(right));
    });
    return frames;
}

void RunAnimationImport(const AnimationJob& job, BatchProgress& progress) {
    const BatchJob& conversion = job.conversion;
    std::vector<graphics::Image> frames;
    frames.reserve(conversion.sources.size());

    for (size_t index = 0; index < conversion.sources.size(); ++index) {
        std::string error;
        graphics::Image source;
        if (!formats::LoadImageFile(conversion.sources[index], source, &error)) {
            Fail(progress, Utf8FileName(conversion.sources[index]) + ": " + error);
            return;
        }
        frames.push_back(ConvertBatchSource(conversion, index, source));
        ++progress.processed;
    }

    std::string error;
    if (!formats::SaveAsepriteAnimation(frames, job.frameDurationMs, job.outputPath, &error)) {
        Fail(progress, error);
        return;
    }
    progress.saved = static_cast<int>(frames.size());
}

} // namespace pixie::services
