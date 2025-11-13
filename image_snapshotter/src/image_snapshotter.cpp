#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "image_snapshotter_interfaces/srv/get_still.hpp"
#include "rclcpp/node.hpp"
#include "rclcpp/node_options.hpp"
#include "rclcpp/service.hpp"
#include "rclcpp/subscription.hpp"
#include "rclcpp/timer.hpp"
#include "rmw/types.h"
#include "sensor_msgs/msg/compressed_image.hpp"

namespace image_snapshotter {
class ImageSnapshotter : public rclcpp::Node {
public:
  explicit ImageSnapshotter(const rclcpp::NodeOptions &options)
      : Node("image_snapshotter", options) {
    auto fq_topic = this->get_node_topics_interface()->resolve_topic_name(
        "image_raw/compressed");

    while (this->count_publishers("image_raw/compressed") == 0) {
      if (!rclcpp::ok()) {
        RCLCPP_ERROR(this->get_logger(),
                     "Interrupted while waiting for the publisher. Exiting.");
        return;
      }
      RCLCPP_WARN(this->get_logger(), "Waiting for a publisher on %s topic...",
                  fq_topic.c_str());
      rclcpp::sleep_for(std::chrono::seconds(1));
    }
    RCLCPP_INFO(this->get_logger(), "Publisher found. Ready to get stills.");

    get_still_service_ =
        this->create_service<image_snapshotter_interfaces::srv::GetStill>(
            "get_still",
            std::bind(&ImageSnapshotter::handle_get_still, this,
                      std::placeholders::_1, std::placeholders::_2));
  }

private:
  void
  handle_get_still(const std::shared_ptr<rmw_request_id_t> request_header,
                   const std::shared_ptr<
                       image_snapshotter_interfaces::srv::GetStill::Request>
                       request) {
    RCLCPP_INFO(this->get_logger(), "Received get_still request");

    if (stored_request_header_ != nullptr) {
      std::string error_msg = "A previous request is still being processed. "
                              "Ignoring this new request.";
      RCLCPP_WARN(this->get_logger(), "%s", error_msg.c_str());
      image_snapshotter_interfaces::srv::GetStill::Response response;
      response.success = false;
      response.reason = std::move(error_msg);
      get_still_service_->send_response(*request_header, response);
      return;
    }

    stored_request_header_ = request_header;
    double timeout = request->timeout == 0.0 ? 2.0 : request->timeout;
    timeout_timer_ = this->create_wall_timer(
        std::chrono::duration<double>(timeout),
        std::bind(&ImageSnapshotter::handle_timeout, this));

    image_subscription_ =
        this->create_subscription<sensor_msgs::msg::CompressedImage>(
            "image_raw/compressed", 1,
            std::bind(&ImageSnapshotter::handle_image, this,
                      std::placeholders::_1));
  }

  void handle_timeout() {
    RCLCPP_WARN(this->get_logger(), "GetStill request timed out.");

    if (stored_request_header_ == nullptr) {
      RCLCPP_WARN(this->get_logger(),
                  "No stored request header found on timeout. Ignoring.");
      return;
    }

    if (stored_request_header_ != nullptr) {
      image_snapshotter_interfaces::srv::GetStill::Response response;
      response.success = false;
      response.reason = "Request timed out waiting for an image.";
      get_still_service_->send_response(*stored_request_header_, response);
      stored_request_header_ = nullptr;
    }
    timeout_timer_->cancel();
    timeout_timer_ = nullptr;
    image_subscription_ = nullptr;
    stored_request_header_ = nullptr;
  }

  void handle_image(sensor_msgs::msg::CompressedImage::UniquePtr msg) {
    RCLCPP_INFO(this->get_logger(), "Received image, sending response.");

    if (stored_request_header_ == nullptr) {
      RCLCPP_ERROR(this->get_logger(),
                   "No stored request header found. Ignoring received image.");
      return;
    }

    image_snapshotter_interfaces::srv::GetStill::Response response;
    response.success = true;
    response.still.header = msg->header;
    response.still.format = msg->format;
    response.still.data = std::move(msg->data);
    get_still_service_->send_response(*stored_request_header_, response);

    timeout_timer_->cancel();
    timeout_timer_ = nullptr;
    image_subscription_ = nullptr;
    stored_request_header_ = nullptr;
  }

  std::shared_ptr<rclcpp::Service<image_snapshotter_interfaces::srv::GetStill>>
      get_still_service_;
  std::shared_ptr<rmw_request_id_t> stored_request_header_;
  rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr
      image_subscription_;
  rclcpp::TimerBase::SharedPtr timeout_timer_;
};

}  // namespace image_snapshotter

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(image_snapshotter::ImageSnapshotter)
