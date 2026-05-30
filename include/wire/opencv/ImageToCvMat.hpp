#pragma once

// ImageToCvMat — adapter node that converts a flow::Image into a cv::Mat
// suitable for the opencv_nodes module's BGR-domain operations.  Branches
// on the source kind:
//   * Kind::Encoded   → cv::imdecode (PNG/JPEG/…)
//   * Kind::RawRGBA   → wrap bytes as a 4-channel header, convert to BGR
//
// Output is BGR (3-channel, CV_8UC3) for the common case so that downstream
// opencv_nodes Compute() bodies work without extra channel-order handling.
// A future variant could expose an output-format input; not needed today.
//
// Only compiled into wire when FLOW_INTERWORK_WITH_OPENCV is set —
// this header should never be included on builds without OpenCV available.

#include <flow/core/Env.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/Node.hpp>
#include <flow/core/TypeName.hpp>
#include <flow/core/UUID.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <wire/Image.hpp>

FLOW_NAMESPACE_BEGIN

class ImageToCvMat;
template<>
constexpr std::string_view TypeName_v<ImageToCvMat> = "ImageToCvMat";

class ImageToCvMat : public Node
{
  public:
    explicit ImageToCvMat(const UUID& uuid, const std::string& name, std::shared_ptr<Env> env)
        : Node(uuid, TypeName_v<ImageToCvMat>, name, std::move(env))
    {
        AddInput<Image>("image", "Input flow::Image (Encoded or RawRGBA)");
        AddOutput<cv::Mat>("mat", "Decoded BGR image (CV_8UC3)");
    }

    ~ImageToCvMat() override = default;

  protected:
    void Compute() override
    {
        auto data = GetInputData<Image>(IndexableName{"image"});
        if (!data)
        {
            return; // no input yet; downstream stays at its default
        }
        const Image& img = data->Get();
        if (!img.bytes || img.bytes->empty())
        {
            return;
        }

        cv::Mat out;

        if (img.kind == Image::Kind::Encoded)
        {
            // imdecode allocates its own storage and resolves dimensions
            // from the file headers.  IMREAD_COLOR returns BGR which is the
            // OpenCV idiom.
            cv::Mat src(1, static_cast<int>(img.bytes->size()), CV_8UC1, img.bytes->data());
            out = cv::imdecode(src, cv::IMREAD_COLOR);
            if (out.empty())
            {
                throw std::runtime_error("ImageToCvMat: cv::imdecode failed (unsupported format or corrupt bytes)");
            }
        }
        else
        { // Kind::RawRGBA
            if (img.width <= 0 || img.height <= 0)
            {
                throw std::runtime_error("ImageToCvMat: RawRGBA input is missing width/height");
            }
            // Wrap (zero-copy) then convert.  The wrapper Mat shares the
            // shared_ptr's buffer; cvtColor allocates `out` separately so
            // the buffer can be released safely after Compute returns.
            const int stride = img.row_stride > 0 ? img.row_stride : img.width * 4;
            cv::Mat rgba_view(img.height, img.width, CV_8UC4, img.bytes->data(), static_cast<std::size_t>(stride));
            cv::cvtColor(rgba_view, out, cv::COLOR_RGBA2BGR);
        }

        SetOutputData(IndexableName{"mat"}, std::make_shared<NodeData<cv::Mat>>(std::move(out)));
    }
};

FLOW_NAMESPACE_END
