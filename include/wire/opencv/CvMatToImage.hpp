#pragma once

// CvMatToImage — adapter node that converts a cv::Mat (the common output of
// opencv_nodes Compute() bodies) into a tightly-packed RawRGBA flow::Image
// suitable for the Flutter PreviewNode renderer (and any other consumer
// speaking the canonical wire type).
//
// Channel handling branches on cv::Mat::channels():
//   * 1 (grayscale) → COLOR_GRAY2RGBA
//   * 3 (BGR)       → COLOR_BGR2RGBA
//   * 4 (BGRA)      → COLOR_BGRA2RGBA
// Anything else (e.g. CV_16U, CV_32F, exotic layouts) throws — downstream
// renderers only know how to handle 8-bit RGBA.
//
// Only compiled into wire when FLOW_INTERWORK_WITH_OPENCV is set.

#include <flow/core/Env.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/Node.hpp>
#include <flow/core/TypeName.hpp>
#include <flow/core/UUID.hpp>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <wire/Image.hpp>

FLOW_NAMESPACE_BEGIN

class CvMatToImage;
template<>
constexpr std::string_view TypeName_v<CvMatToImage> = "CvMatToImage";

class CvMatToImage : public Node
{
  public:
    explicit CvMatToImage(const UUID& uuid, const std::string& name, std::shared_ptr<Env> env)
        : Node(uuid, TypeName_v<CvMatToImage>, name, std::move(env))
    {
        AddInput<cv::Mat>("mat", "Input image (CV_8U with 1, 3, or 4 channels)");
        AddOutput<Image>("image", "RawRGBA flow::Image (RGBA8888, tightly packed)");
    }

    ~CvMatToImage() override = default;

  protected:
    void Compute() override
    {
        auto data = GetInputData<cv::Mat>(IndexableName{"mat"});
        if (!data)
        {
            return;
        }
        const cv::Mat& src = data->Get();
        if (src.empty())
        {
            return;
        }
        if (src.depth() != CV_8U)
        {
            throw std::runtime_error("CvMatToImage: only 8-bit depth (CV_8U) is supported");
        }

        // Convert to RGBA in a fresh Mat that we own outright.  We then
        // copy into a vector so the buffer's lifetime is independent of
        // any OpenCV reference counting (flow::Image holds the shared_ptr).
        cv::Mat rgba;
        switch (src.channels())
        {
        case 1:
            cv::cvtColor(src, rgba, cv::COLOR_GRAY2RGBA);
            break;
        case 3:
            cv::cvtColor(src, rgba, cv::COLOR_BGR2RGBA);
            break;
        case 4:
            cv::cvtColor(src, rgba, cv::COLOR_BGRA2RGBA);
            break;
        default:
            throw std::runtime_error("CvMatToImage: unsupported channel count " + std::to_string(src.channels()));
        }

        const int width      = rgba.cols;
        const int height     = rgba.rows;
        const int row_stride = width * 4;

        auto bytes = std::make_shared<std::vector<uint8_t>>(static_cast<std::size_t>(row_stride) *
                                                            static_cast<std::size_t>(height));

        // Copy row-by-row so we always end up tightly packed regardless of
        // OpenCV's internal step (which can exceed cols*channels due to
        // alignment).
        for (int y = 0; y < height; ++y)
        {
            std::memcpy(bytes->data() + y * row_stride, rgba.ptr<uint8_t>(y), static_cast<std::size_t>(row_stride));
        }

        Image out;
        out.kind            = Image::Kind::RawRGBA;
        out.format          = Image::Format::RGBA8888;
        out.width           = width;
        out.height          = height;
        out.row_stride      = row_stride;
        out.bytes           = std::move(bytes);
        out.content_version = next_version();

        SetOutputData(IndexableName{"image"}, std::make_shared<NodeData<Image>>(std::move(out)));
    }

  private:
    static uint64_t next_version() noexcept
    {
        static std::atomic<uint64_t> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed) + 1;
    }
};

FLOW_NAMESPACE_END
