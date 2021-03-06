/*
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * ROS Includes
 */
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joy.hpp"

#include "hri_c_driver/VehicleMessages.h"
#include "JoystickHandler.hpp"

namespace hri_safety_sense {

JoystickHandler::JoystickHandler(
  rclcpp::node_interfaces::NodeTopicsInterface::SharedPtr nodeTopics,
  rclcpp::node_interfaces::NodeLoggingInterface::SharedPtr nodeLogger,
  rclcpp::node_interfaces::NodeClockInterface::SharedPtr nodeClock,
  const std::string &frameId) :
  nodeLogger_(nodeLogger), nodeClock_(nodeClock), frameId_(frameId)
{
  // Joystick Pub
  rawLeftPub_ = rclcpp::create_publisher<sensor_msgs::msg::Joy>(nodeTopics,
    "/joy", 10);
}

float JoystickHandler::getStickValue(const JoystickType &joystick)
{
  int32_t magnitude = (joystick.magnitude<<2) + joystick.mag_lsb;

  if(joystick.neutral_status == STATUS_SET) {
    return 0;
  }
  if(joystick.negative_status == STATUS_SET) {
    return static_cast<float>(-1 * magnitude);
  }
  if(joystick.positive_status == STATUS_SET) {
    return static_cast<float>(magnitude);
  }

  // Error case
  return 0;
}

int32_t JoystickHandler::getButtonValue(const uint8_t &button)
{
  if(button == STATUS_SET) {
    return 1;
  }

  // Error case
  return 0;
}

uint32_t JoystickHandler::handleNewMsg(const VscMsgType &incomingMsg)
{
  int retval = 0;

  if(incomingMsg.msg.meta.length == sizeof(JoystickMsgType)) {

    JoystickMsgType *joyMsg = reinterpret_cast<JoystickMsgType*>(
      const_cast<uint8_t*>(incomingMsg.msg.meta.data));

    // Broadcast Left Joystick
    sensor_msgs::msg::Joy sendLeftMsg;

    sendLeftMsg.header.stamp = this->nodeClock_->get_clock()->now();
    sendLeftMsg.header.frame_id = this->frameId_;

    // The Left/Right on the HRI is -1023 for fully left and 1023 for fully right.
    // In order to conform to the standard joystick values, we invert this and
    // normalize between 1.0 (fully left) and -1.0 (fully right).
    sendLeftMsg.axes.push_back(-getStickValue(joyMsg->leftX) / this->AXIS_MAX);
    sendLeftMsg.axes.push_back(getStickValue(joyMsg->leftY) / this->AXIS_MAX);
    sendLeftMsg.axes.push_back(getStickValue(joyMsg->leftZ) / this->AXIS_MAX);

    sendLeftMsg.buttons.push_back(getButtonValue(joyMsg->leftSwitch.home));
    sendLeftMsg.buttons.push_back(getButtonValue(joyMsg->leftSwitch.first));
    sendLeftMsg.buttons.push_back(getButtonValue(joyMsg->leftSwitch.second));
    sendLeftMsg.buttons.push_back(getButtonValue(joyMsg->leftSwitch.third));

    // See above for an explanation of the negative sign.
    sendLeftMsg.axes.push_back(-getStickValue(joyMsg->rightX) / this->AXIS_MAX);
    sendLeftMsg.axes.push_back(getStickValue(joyMsg->rightY) / this->AXIS_MAX);
    sendLeftMsg.axes.push_back(getStickValue(joyMsg->rightZ) / this->AXIS_MAX);

    sendLeftMsg.buttons.push_back(getButtonValue(joyMsg->rightSwitch.home));
    sendLeftMsg.buttons.push_back(getButtonValue(joyMsg->rightSwitch.first));
    sendLeftMsg.buttons.push_back(getButtonValue(joyMsg->rightSwitch.second));
    sendLeftMsg.buttons.push_back(getButtonValue(joyMsg->rightSwitch.third));

    rawLeftPub_->publish(sendLeftMsg);

  } else {
    retval = -1;

    RCLCPP_WARN(this->nodeLogger_->get_logger(),
      "RECEIVED PTZ COMMANDS WITH INVALID MESSAGE SIZE! Expected: 0x%x, Actual: 0x%x",
      static_cast<unsigned int>(sizeof(JoystickMsgType)),
      incomingMsg.msg.meta.length);
  }

  return retval;
}

}  // namespace hri_safety_sense
