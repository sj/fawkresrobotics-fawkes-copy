/***************************************************************************
 *  gazsim_vis_localization_thread.h - Plugin visualizes the localization
 *
 *  Created: Tue Sep 17 15:36:25 2013
 *  Copyright  2013  Frederik Zwilling
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL file in the doc directory.
 */

#ifndef __PLUGINS_GAZSIM_VIS_LOCALIZATION_THREAD_H_
#define __PLUGINS_GAZSIM_VIS_LOCALIZATION_THREAD_H_

#include <core/threading/thread.h>
#include <aspect/clock.h>
#include <aspect/configurable.h>
#include <aspect/logging.h>
#include <aspect/blackboard.h>
#include <aspect/blocked_timing.h>
#include <plugins/gazebo/aspect/gazebo.h>
#include <utils/time/clock.h>
#include <utils/time/time.h>
#include <string.h>

//from Gazebo
#include <gazebo/transport/TransportTypes.hh>
#include <gazebo/msgs/MessageTypes.hh>
#include <gazebo/transport/transport.hh>


namespace fawkes {
  class Position3DInterface;
}

class VisLocalizationThread
: public fawkes::Thread,
  public fawkes::ClockAspect,
  public fawkes::LoggingAspect,
  public fawkes::ConfigurableAspect,
  public fawkes::BlackBoardAspect,
  public fawkes::BlockedTimingAspect,
  public fawkes::GazeboAspect
{
 public:
  VisLocalizationThread();

  virtual void init();
  virtual void loop();
  virtual void finalize();

 private:
  //read pose interface
  fawkes::Position3DInterface *pose_if_;

  //Publisher for visual msgs
  gazebo::transport::PublisherPtr visual_publisher_;

  double update_rate_;
  fawkes::Time last_update_time_;

  //config values
  std::string robot_name_, label_script_name_, location_scripts_, location_textures_, parent_name_, arrow_script_name_;
  float label_size_;
  float label_height_;
};
#endif
