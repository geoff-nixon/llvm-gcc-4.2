/* URLStreamHandlerCache.java -- a cache for URLStreamHandlers
   Copyright (C) 2006 Free Software Foundation, Inc.

This file is part of GNU Classpath.

GNU Classpath is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Classpath is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Classpath; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301 USA.

Linking this library statically or dynamically with other modules is
making a combined work based on this library.  Thus, the terms and
conditions of the GNU General Public License cover the whole
combination.

As a special exception, the copyright holders of this library give you
permission to link this library with independent modules to produce an
executable, regardless of the license terms of these independent
modules, and to copy and distribute the resulting executable under
terms of your choice, provided that you also meet, for each linked
independent module, the terms and conditions of the license of that
module.  An independent module is a module which is not derived from
or based on this library.  If you modify this library, you may extend
this exception to your version of the library, but you are not
obligated to do so.  If you do not wish to do so, delete this
exception statement from your version. */


package gnu.java.net.loader;

import java.net.URLStreamHandler;
import java.net.URLStreamHandlerFactory;
import java.util.HashMap;

/**
 */
public class URLStreamHandlerCache
{
  /**
   * A cache to store mappings between handler factory and its
   * private protocol handler cache (also a HashMap), so we can avoid
   * creating handlers each time the same protocol comes.
   */
  private HashMap factoryCache = new HashMap(5);

  public URLStreamHandlerCache()
  {
  }

  public synchronized void add(URLStreamHandlerFactory factory)
  {
    // Since we only support three protocols so far, 5 is enough
    // for cache initial size.
    if (factory != null && factoryCache.get(factory) == null)
      factoryCache.put(factory, new HashMap(5));
  }

  public synchronized URLStreamHandler get(URLStreamHandlerFactory factory,
                                           String protocol)
  {
    if (factory == null)
      return null;
    // Check if there're handler for the same protocol in cache.
    HashMap cache = (HashMap) factoryCache.get(factory);
    URLStreamHandler handler = (URLStreamHandler) cache.get(protocol);
    if (handler == null)
      {
        // Add it to cache.
        handler = factory.createURLStreamHandler(protocol);
        cache.put(protocol, handler);
      }
    return handler;
  }
}
