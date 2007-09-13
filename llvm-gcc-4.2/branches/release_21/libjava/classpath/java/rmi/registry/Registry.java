/* Registry.java --
   Copyright (c) 1996, 1997, 1998, 1999, 2004  Free Software Foundation, Inc.

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


package java.rmi.registry;

import java.rmi.AccessException;
import java.rmi.AlreadyBoundException;
import java.rmi.NotBoundException;
import java.rmi.Remote;
import java.rmi.RemoteException;

public interface Registry extends Remote
{
  int REGISTRY_PORT = 1099;
  
  /**
   * Find and return the reference to the object that was previously bound
   * to the registry by this name. For remote objects, this method returns
   * the stub instances, containing the code for remote invocations.
   * 
   * Since jdk 1.5 this method does not longer require the stub class 
   * (nameImpl_Stub) to be present. If such class is not found, the stub is 
   * replaced by the dynamically constructed proxy class. No attempt to find 
   * and load the stubs is made if the system property 
   * java.rmi.server.ignoreStubClasses is set to true (set to reduce the 
   * starting time if the stubs are surely not present and exclusively 1.2
   * RMI is used).
   * 
   * @param name the name of the object
   * 
   * @return the reference to that object on that it is possible to invoke
   * the (usually remote) object methods.
   * 
   * @throws RemoteException
   * @throws NotBoundException
   * @throws AccessException
   */
  Remote lookup(String name)
    throws RemoteException, NotBoundException, AccessException;

  void bind(String name, Remote obj)
    throws RemoteException, AlreadyBoundException, AccessException;

  void unbind(String name)
    throws RemoteException, NotBoundException, AccessException;

  void rebind(String name, Remote obj)
    throws RemoteException, AccessException;

  String[] list()
    throws RemoteException, AccessException;
}
