/***************************************************************************
 *  exceptions.h - SyncPoint exceptions
 *
 *  Created: Wed Jan 15 11:09:55 2014
 *  Copyright  2014  Till Hofmann
 *
 *
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

#ifndef __SYNCPOINT_EXCEPTIONS_H_
#define __SYNCPOINT_EXCEPTIONS_H_

#include <core/exception.h>

namespace fawkes {

/** A component which is watching a SyncPoint, called get_syncpoint() for the
 * same identifier
 */
class SyncPointAlreadyOpenedException : public Exception
{
  public:
    /** Constructor.
     * @param component The calling component
     * @param identifier The identifier of the SyncPoint
     */
    SyncPointAlreadyOpenedException(const char * component,
      const char *identifier)
    {
      append(
          "Component '%s' called get_syncpoint() for identifier '%s', but is already watching",
          component, identifier);
    }
};

/** Emit was called by a component which isn't in the watcher set
 * (or wrong component argument was passed)
 */
class SyncPointNonWatcherCalledEmitException : public Exception
{
  public:
    /** Constructor.
     * @param component The calling component
     * @param identifier The identifier of the SyncPoint
     */
    SyncPointNonWatcherCalledEmitException(const char * component,
      const char *identifier)
    {
      append("Component '%s' called emit for SyncPoint '%s', but is not a watcher",
          component, identifier);
    }
};

/** Emit was called by a component which isn't in the watcher set
 * (or wrong component argument was passed)
 */
class SyncPointNonWatcherCalledWaitException : public Exception
{
  public:
    /** Constructor.
     * @param component The calling component
     * @param identifier The identifier of the SyncPoint
     */
    SyncPointNonWatcherCalledWaitException(const char * component,
      const char *identifier)
    {
      append("Component '%s' called wait for SyncPoint '%s', but is not a watcher",
          component, identifier);
    }
};

/** Release was called on a non-existing SyncPoint
 *
 */
class SyncPointReleasedDoesNotExistException : public Exception
{
  public:
    /** Constructor.
     * @param component The calling component
     * @param identifier The identifier of the SyncPoint
     */
    SyncPointReleasedDoesNotExistException(const char * component,
      const char * identifier)
    {
      append("Component '%s' tried to release non-existing SyncPoint '%s'",
          component, identifier);
    }
};

/** Release was called by a component which isn't a watcher
 *
 */
class SyncPointReleasedByNonWatcherException : public Exception
{
  public:
    /** Constructor.
     * @param component The calling component
     * @param identifier The identifier of the SyncPoint
     */
    SyncPointReleasedByNonWatcherException(const char * component,
      const char * identifier)
    {
      append("Component '%s' tried to release SyncPoint '%s' but is not a watcher",
          component, identifier);
    }
};

/** Invalid identifier used (i.e. an empty string)
 *
 */
class SyncPointInvalidIdentifierException : public Exception
{
  public:
    /** Constructor.
     * @param identifier The identifier of the SyncPoint
     */
    SyncPointInvalidIdentifierException(const char * identifier)
    {
      append("Tried to construct a SyncPoint with invalid identifier ('%s'). "
          "Identifier must be a non-empty absolute path (e.g. '/path/to/syncpoint')"
          " and may not end with '/'",
          identifier);
    }
};

/** Invalid component name used (i.e. an empty string)
 *
 */
class SyncPointInvalidComponentException : public Exception
{
  public:
    /** Constructor.
     * @param component The calling component
     * @param identifier The identifier of the SyncPoint
     */
    SyncPointInvalidComponentException(const char * component,
      const char * identifier)
    {
      append("Invalid component name '%s' while accessing SyncPoint '%s'",
          component, identifier);
    }
};

/** A component called wait() but is already waiting
 *
 */
class SyncPointMultipleWaitCallsException : public Exception
{
  public:
    /** Constructor.
     * @param component The calling component
     * @param identifier The identifier of the SyncPoint
     */
    SyncPointMultipleWaitCallsException(const char * component,
      const char * identifier)
    {
      append("Component '%s' called wait() on SyncPoint '%s', but is already waiting",
          component, identifier);
    }
};

/** Emit was called on a SyncBarrier but the calling component is not registered
 *  as emitter
 */
class SyncPointNonEmitterCalledEmitException : public Exception
{
  public:
    /** Constructor.
     * @param component The calling component
     * @param identifier The identifier of the SyncPoint
     */
	SyncPointNonEmitterCalledEmitException(const char * component,
      const char *identifier)
    {
      append("Component '%s' called emit for SyncPoint '%s', "
          "but is not a registered emitter", component, identifier);
    }
};

/** Invalid SyncPoint type.
 */
class SyncPointInvalidTypeException : public Exception
{
public:
  /** Constructor. */
  SyncPointInvalidTypeException()
  {
    append("Invalid SyncPoint Wakeup type.");
  }
};

/** The component called release but is still registered as emitter. */
class SyncPointCannotReleaseEmitter : public Exception
{
  public:
  /** Constructor.
   *  @param component The calling component
   *  @param identifier The identifier of the SyncPoint
   */
SyncPointCannotReleaseEmitter(const char * component,
    const char * identifier)
  {
    append("Component '%s' called emit for SyncPoint '%s', "
        "but is still registered as emitter", component, identifier);
  }
};


} // namespace fawkes


#endif
