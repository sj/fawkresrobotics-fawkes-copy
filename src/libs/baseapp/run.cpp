
/***************************************************************************
 *  run.cpp - Fawkes run functions
 *
 *  Created: Wed May 04 23:23:23 2011
 *  Copyright  2006-2011  Tim Niemueller [www.niemueller.de]
 *
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version. A runtime exception applies to
 *  this software (see LICENSE.GPL_WRE file mentioned below for details).
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL_WRE file in the doc directory.
 */

#include <baseapp/run.h>
#include <baseapp/daemonize.h>
#include <baseapp/main_thread.h>
#include <baseapp/thread_manager.h>

#include <core/threading/thread.h>

#include <blackboard/local.h>
#include <config/sqlite.h>
#include <config/yaml.h>
#include <config/net_handler.h>
#include <utils/ipc/shm.h>
#include <utils/system/argparser.h>
#include <logging/multi.h>
#include <logging/console.h>
#include <logging/liblogger.h>
#include <logging/factory.h>
#include <network_logger/network_logger.h>
#ifdef HAVE_LOGGING_FD_REDIRECT
#  include <logging/fd_redirect.h>
#endif
#include <utils/time/clock.h>
#include <utils/time/time.h>
#include <netcomm/fawkes/network_manager.h>
#include <plugin/manager.h>
#include <plugin/net/handler.h>
#include <aspect/manager.h>
#include <syncpoint/syncpoint_manager.h>
#ifdef HAVE_TF
#  include <tf/transform_listener.h>
#  include <tf/transformer.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <fnmatch.h>

namespace fawkes {
  namespace runtime {
#if 0 /* just to make Emacs auto-indent happy */
  }
}
#endif

ArgumentParser            * argument_parser = NULL;
FawkesMainThread          * main_thread = NULL;
MultiLogger               * logger = NULL;
NetworkLogger             * network_logger = NULL;
BlackBoard                * blackboard = NULL;
Configuration             * config = NULL;
PluginManager             * plugin_manager = NULL;
AspectManager             * aspect_manager = NULL;
ThreadManager             * thread_manager = NULL;
FawkesNetworkManager      * network_manager = NULL;
ConfigNetworkHandler      * nethandler_config = NULL;
PluginNetworkHandler      * nethandler_plugin = NULL;
Clock                     * clock = NULL;
SharedMemoryRegistry      * shm_registry;
InitOptions               * init_options = NULL;
tf::Transformer           * tf_transformer = NULL;
tf::TransformListener     * tf_listener = NULL;
Time                      * start_time = NULL;
SyncPointManager          * syncpoint_manager = NULL;
#ifdef HAVE_LOGGING_FD_REDIRECT
LogFileDescriptorToLog    * log_fd_redirect_stderr_ = NULL;
LogFileDescriptorToLog    * log_fd_redirect_stdout_ = NULL;
#endif

// this is NOT shared to the outside
FawkesMainThread::Runner  * runner = NULL;

bool
init(int argc, char **argv, int & retval)
{
  return init(InitOptions(argc, argv), retval);
}


bool
init(InitOptions options, int & retval)
{
  init_options = new InitOptions(options);

  if (init_options->show_help())  return true;

  if ( options.daemonize() ) {
    fawkes::daemon::init(options.daemon_pid_file(), options.basename());
    if (options.daemonize_kill()) {
      fawkes::daemon::kill();
      retval = 0;
      return false;
    } else if (options.daemonize_status()) {
      retval = fawkes::daemon::running() ? 0 : 1;
      return false;
    } else {
      if (fawkes::daemon::start()) {
	retval = 0;
	return false;
      }
    }
  }

  // *** set user group if requested
  const char *user  = NULL;
  const char *group = NULL;
  if (options.has_username()) {
    user = options.username();
  }
  if (options.has_groupname()) {
    group = options.groupname();
  }

  if (user != NULL) {
    struct passwd *pw;
    if (! (pw = getpwnam(user))) {
      printf("Failed to find user %s, check -u argument.\n", user);
      retval = 203;
      return false;
    }
    int r = 0;
    r = setreuid(pw->pw_uid, pw->pw_uid);
    if (r < 0) {
      perror("Failed to drop privileges (user)");
    }
  }

  if (group != NULL) {
    struct group *gr;
    if (! (gr = getgrnam(group))) {
      printf("Failed to find group %s, check -g argument.\n", user);
      retval = 204;
      return false;
    }
    int r = 0;
    r = setregid(gr->gr_gid, gr->gr_gid);
    if (r < 0) {
      perror("Failed to drop privileges (group)");
    }
  }

  // *** setup base thread and shm registry
  Thread::init_main();

  shm_registry = NULL;
  struct passwd *uid_pw = getpwuid(getuid());
  if (uid_pw == NULL) {
    shm_registry = new SharedMemoryRegistry();
  } else {
    char *registry_name;
    if (asprintf(&registry_name, USER_SHM_NAME, uid_pw->pw_name) == -1) {
      shm_registry = new SharedMemoryRegistry();
    } else {
      shm_registry = new SharedMemoryRegistry(registry_name);
      free(registry_name);
    }
  }

  if (! shm_registry) {
    throw Exception("Failed to create shared memory registry");
  }

  // *** setup logging
  if (options.has_loggers()) {
    try {
      logger = LoggerFactory::multilogger_instance(options.loggers());
    } catch (Exception &e) {
      e.append("Initializing multi logger failed");
      throw;
    }
  } else {
    logger = new MultiLogger(new ConsoleLogger());
  }

  logger->set_loglevel(options.log_level());
  LibLogger::init(logger);

  // *** Prepare home dir directory, just in case
  const char *homedir = getenv("HOME");
  if (homedir) {
    char *userdir;
    if (asprintf(&userdir, "%s/%s", homedir, USERDIR) != -1) {
      if (access(userdir, W_OK) != 0) {
	if (mkdir(userdir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
	  logger->log_warn("FawkesMainThread", "Failed to create .fawkes "
			   "directory %s, trying without", userdir);
	}
      }
      free(userdir);
    }
  }

  // *** setup config
  
  SQLiteConfiguration *sqconfig = NULL;

  if (options.config_file() &&
      fnmatch("*.sql", options.config_file(), FNM_PATHNAME) == 0)
  {
    sqconfig = new SQLiteConfiguration(CONFDIR);
    config = sqconfig;
  } else {
    config = new YamlConfiguration(CONFDIR);
  }

  config->load(options.config_file());

  if (sqconfig) {
    try {
      SQLiteConfiguration::SQLiteValueIterator *i = sqconfig->modified_iterator();
      while (i->next()) {
	std::string modtype = i->get_modtype();
	if (modtype == "changed") {
	  logger->log_warn("FawkesMainThread", "Default config value CHANGED: %s"
			   "(was: %s now: %s)", i->path(),
			   i->get_oldvalue().c_str(), i->get_as_string().c_str());
	} else if (modtype == "erased") {
	  logger->log_warn("FawkesMainThread", "Default config value ERASED:  %s",
			   i->path());
	} else {
	  logger->log_debug("FawkesMainThread", "Default config value ADDED:   %s "
			    "(value: %s)", i->path(), i->get_as_string().c_str());
	}
      }
      delete i;
    } catch (Exception &e) {
      logger->log_warn("FawkesMainThread", "Failed to read modified default "
		       "config values, no dump?");
    }
  }

  if (! options.has_loggers()) {
    // Allow configuration override from config
    if (config->exists("/fawkes/mainapp/loggers")) {
      try {
        std::string loggers = config->get_string("/fawkes/mainapp/loggers");
        MultiLogger *new_logger =
	  LoggerFactory::multilogger_instance(loggers.c_str(), options.log_level());
        logger = new_logger;
        LibLogger::finalize();
        LibLogger::init(new_logger);
      } catch (Exception &e) {
        logger->log_warn("FawkesMainThread", "Loggers set in config file, "
			 "but failed to read, exception follows.");
        logger->log_warn("FawkesMainThread", e);
      }
    }
  }

  if (config->exists("/fawkes/mainapp/log_stderr_as_warn")) {
    try {
      bool log_stderr_as_warn = config->get_bool("/fawkes/mainapp/log_stderr_as_warn");
      if (log_stderr_as_warn) {
#ifdef HAVE_LOGGING_FD_REDIRECT
	log_fd_redirect_stderr_ =
	  new LogFileDescriptorToLog(STDERR_FILENO, logger, "stderr", Logger::LL_WARN);
#else
	logger->log_warn("FawkesMainThread", "stderr log redirection enabled but not available at compile time");
#endif
      }
    } catch (Exception &e) {} // ignored
  }

  // *** Determine network parameters
  bool enable_ipv4 = true;
  bool enable_ipv6 = true;
  std::string listen_ipv4;
  std::string listen_ipv6;
  unsigned int net_tcp_port     = 1910;
  std::string  net_service_name = "Fawkes on %h";
  if (options.has_net_tcp_port()) {
    net_tcp_port = options.net_tcp_port();
  } else {
    try {
      net_tcp_port = config->get_uint("/network/fawkes/tcp_port");
    } catch (Exception &e) {}  // ignore, we stick with the default
  }

  if (options.has_net_service_name()) {
    net_service_name = options.net_service_name();
  } else {
    try {
      net_service_name = config->get_string("/network/fawkes/service_name");
    } catch (Exception &e) {}  // ignore, we stick with the default
  }

  if (net_tcp_port > 65535) {
    logger->log_warn("FawkesMainThread", "Invalid port '%u', using 1910",
		     net_tcp_port);
    net_tcp_port = 1910;
  }

  try {
	  enable_ipv4 = config->get_bool("/network/ipv4/enable");
  } catch (Exception &e) {}  // ignore, we stick with the default
  try {
	  enable_ipv6 = config->get_bool("/network/ipv6/enable");
  } catch (Exception &e) {}  // ignore, we stick with the default

  try {
	  listen_ipv4 = config->get_string("/network/ipv4/listen");
  } catch (Exception &e) {}  // ignore, we stick with the default
  try {
	  listen_ipv6 = config->get_string("/network/ipv6/listen");
  } catch (Exception &e) {}  // ignore, we stick with the default

  if (! enable_ipv4) {
	  logger->log_warn("FawkesMainThread", "Disabling IPv4 support");
  }
  if (! enable_ipv6) {
	  logger->log_warn("FawkesMainThread", "Disabling IPv6 support");
  }
  if (! listen_ipv4.empty()) {
	  logger->log_info("FawkesMainThread", "Listening on IPv4 address %s", listen_ipv4.c_str());
  }
  if (! listen_ipv6.empty()) {
	  logger->log_info("FawkesMainThread", "Listening on IPv6 address %s", listen_ipv4.c_str());
  }
  
  // *** Setup blackboard
  std::string bb_magic_token = "";
  unsigned int bb_size = 2097152;
  try {
    bb_magic_token = config->get_string("/fawkes/mainapp/blackboard_magic_token");
    logger->log_info("FawkesMainApp", "BlackBoard magic token defined. "
		     "Using shared memory BlackBoard.");
  } catch (Exception &e) {
    // ignore
  }
  try {
    bb_size = config->get_uint("/fawkes/mainapp/blackboard_size");
  } catch (Exception &e) {
    logger->log_warn("FawkesMainApp", "BlackBoard size not defined. "
		     "Will use %u, saving to default DB", bb_size);
    config->set_default_uint("/fawkes/mainapp/blackboard_size", bb_size);
  }

  // Cleanup stale BlackBoard shared memory segments if requested
  if ( options.bb_cleanup()) {
    LocalBlackBoard::cleanup(bb_magic_token.c_str(),
			     /* output with lister? */ true);
    SharedMemoryRegistry::cleanup();
  }

  LocalBlackBoard *lbb = NULL;
  if ( bb_magic_token == "") {
    lbb = new LocalBlackBoard(bb_size);
  } else {
    lbb = new LocalBlackBoard(bb_size, bb_magic_token.c_str());
  }
  blackboard = lbb;

#ifdef HAVE_TF
  tf_transformer     = new tf::Transformer();
  tf_listener        = new tf::TransformListener(blackboard, tf_transformer);
#endif

  aspect_manager     = new AspectManager();
  thread_manager     = new ThreadManager(aspect_manager, aspect_manager);

  syncpoint_manager  = new SyncPointManager(logger);

  plugin_manager     = new PluginManager(thread_manager, config,
					 "/fawkes/meta_plugins/",
					 options.plugin_module_flags(),
					 options.init_plugin_cache());
  network_manager    = new FawkesNetworkManager(thread_manager,
                                                enable_ipv4, enable_ipv6,
                                                listen_ipv4, listen_ipv6,
                                                net_tcp_port,
                                                net_service_name.c_str());
  nethandler_config  = new ConfigNetworkHandler(config,
						network_manager->hub());

  nethandler_plugin  = new PluginNetworkHandler(plugin_manager,
						network_manager->hub());
  nethandler_plugin->start();

  network_logger = new NetworkLogger(network_manager->hub(),
				     logger->loglevel());
  logger->add_logger(network_logger);

  clock = Clock::instance();
  start_time = new Time(clock);

  lbb->start_nethandler(network_manager->hub());


  // *** Create main thread, but do not start, yet
  main_thread = new fawkes::FawkesMainThread(config, logger,
					     thread_manager,
					     syncpoint_manager,
					     plugin_manager,
					     options.load_plugin_list(),
                                             options.default_plugin());

  aspect_manager->register_default_inifins(blackboard,
					   thread_manager->aspect_collector(),
					   config, logger, clock,
					   network_manager->hub(),
					   main_thread, logger,
					   thread_manager,
					   network_manager->nnresolver(),
					   network_manager->service_publisher(),
					   network_manager->service_browser(),
					   plugin_manager, tf_transformer,
					   syncpoint_manager);

  retval = 0;
  return true;
}

void
cleanup()
{
  if (init_options->daemonize()) {
    fawkes::daemon::cleanup();
  }

  if (nethandler_plugin) {
    nethandler_plugin->cancel();
    nethandler_plugin->join();
  }

  if (logger) {
    // Must delete network logger first since network manager
    // has to die before the LibLogger is finalized.
    logger->remove_logger(network_logger);
    delete network_logger;
  }

  delete nethandler_config;
  delete nethandler_plugin;
  delete plugin_manager;
  delete main_thread;
#ifdef HAVE_TF
  delete tf_listener;
  delete tf_transformer;
#endif
  delete blackboard;
  delete config;
  delete argument_parser;
  delete init_options;
  delete network_manager;
  delete thread_manager;
  delete aspect_manager;
  delete shm_registry;
#ifdef HAVE_LOGGING_FD_REDIRECT
  delete log_fd_redirect_stderr_;
  delete log_fd_redirect_stdout_;
#endif

  main_thread = NULL;
  argument_parser = NULL;
  init_options = NULL;
  nethandler_config = NULL;
  nethandler_plugin = NULL;
  plugin_manager = NULL;
  network_manager = NULL;
  config = NULL;
  thread_manager = NULL;
  aspect_manager = NULL;
  shm_registry = NULL;
  blackboard = NULL;
#ifdef HAVE_LOGGING_FD_REDIRECT
  log_fd_redirect_stderr_ = NULL;
  log_fd_redirect_stdout_ = NULL;
#endif

  // implicitly frees multi_logger and all sub-loggers
  LibLogger::finalize();
  logger = NULL;

  delete start_time;
  start_time = NULL;
  Clock::finalize();
  clock = NULL;

  try {
    Thread::destroy_main();
  } catch (Exception &e) {} // ignored, can fire on show_help

  // should be last, because of not disabled this hosts the
  // default signal handlers
  delete runner;
  runner = 0;
}

void
run()
{
  if (init_options->show_help()) {
    print_usage(init_options->basename());
    return;
  }

  bool defsigs = init_options->default_signal_handlers();
  runner = new FawkesMainThread::Runner(main_thread, defsigs);

  try {
    runner->run();
  } catch (Exception &e) {
    printf("Running Fawkes failed\n");
    e.print_trace();
  }
}


/** Quit Fawkes.
 * You can call this from within Fawkes to quit Fawkes. Use with extreme care an
 * only rarely.
 * This sends SIGINT to the local process. This triggers the quit routine but also
 * takes a currently running init into account. This is prone to the same potential
 * problems as a SIGINT received otherwise, e.g. a never-ending thread blocking
 * the main thread from cancelling.
 */
void
quit()
{
  kill(getpid(), SIGINT);
}

void
print_usage(const char *progname)
{
  printf("Fawkes Main Application - Usage Instructions\n"
  "================================================"
  "===============================\n"
  "Usage: %s [options] [plugins]\n"
  "where\n"
  " [plugins] is a space-separated list of plugins to load on startup in given order\n"
  " [options] is one or more of:\n"
    "  -h                       These help instructions\n"
    "  -C                       Cleanup old BB and shared memory segments\n"
    "  -c config file           Configuration file to load.\n"
    "                           Examples: default.sql or config.yaml\n"
    "  -d                       Enable debug output\n"
    "  -q[qqq]                  Quiet mode, -q omits debug, -qq debug and"
      "info,\n                           "
      "-qqq omit debug, info and warn, -qqqq no output\n"
    "  -l level                 Set log level directly mutually exclusive"
      "with -q,\n                           "
      "level is one of debug, info, warn, error, or none\n"
    "  -L loggers               Define loggers. By default this setting is"
      "read from\n                           "
      "config (console logger if unset). Format is:\n"
      "                           logger:args[;logger2:args2[!...]]\n"
      "                           Currently supported:\n"
      "                           console, file:file.log, network logger always added\n"
    "  -p plugins               List of plugins to load on startup in given order\n"
    "  -P port                  TCP port to listen on for Fawkes network connections.\n"
      "  --net-service-name=name  mDNS service name to use.\n"
    "  -u user                  Drop privileges and run as given user.\n"
    "  -g group                 Drop privileges and run as given group.\n"
#ifdef HAVE_LIBDAEMON
    "  -D[pid file]             Run daemonized in the background, pid file "
      "is optional,\n                           "
      "default is /var/run/fawkes.pid, must be absolute path.\n"
    "  -D[pid file] -k          Kill a daemonized Fawkes running in the"
      "background\n"
    "  -D[pid file] -s          Check status of daemon.\n"
#endif
  "\n", progname);
}


/** Get Fawkes uptime.
 * Returns the time in seconds since Fawkes was started.
 * This creates a new time for the system clock and subtracts the start time.
 * @return time in seconds since Fawkes was started
 */
float
uptime()
{
  if (start_time) {
    fawkes::Time now(clock);
    return now - start_time;
  } else {
    return 0.0;
  }
}

} // end namespace runtime
} // end namespace fawkes
