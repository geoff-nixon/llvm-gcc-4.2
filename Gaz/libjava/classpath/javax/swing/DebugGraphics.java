/* DebugGraphics.java --
   Copyright (C) 2002, 2004, 2005  Free Software Foundation, Inc.

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

package javax.swing;

import java.awt.Color;
import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Graphics;
import java.awt.Image;
import java.awt.Point;
import java.awt.Rectangle;
import java.awt.Shape;
import java.awt.image.ImageObserver;
import java.io.PrintStream;
import java.text.AttributedCharacterIterator;


/**
 * An extension of {@link Graphics} that can be used for debugging
 * custom Swing widgets. <code>DebugGraphics</code> has the ability to
 * draw slowly and can log drawing actions.
 *
 * @author Andrew Selkirk
 */
public class DebugGraphics extends Graphics
{
  /**
   * LOG_OPTION
   */
  public static final int LOG_OPTION = 1;

  /**
   * FLASH_OPTION
   */
  public static final int FLASH_OPTION = 2;

  /**
   * BUFFERED_OPTION
   */
  public static final int BUFFERED_OPTION = 4;

  /**
   * NONE_OPTION
   */
  public static final int NONE_OPTION = -1;

  static Color debugFlashColor = Color.RED;
  static int debugFlashCount = 10;
  static int debugFlashTime = 1000;
  static PrintStream debugLogStream = System.out;

  /**
   * Counts the created DebugGraphics objects. This is used by the
   * logging facility.
   */
  static int counter = 0;

  /**
   * graphics
   */
  Graphics graphics;

  /**
   * buffer
   */
  Image buffer;

  /**
   * debugOptions
   */
  int debugOptions;

  /**
   * graphicsID
   */
  int graphicsID;

  /**
   * xOffset
   */
  int xOffset;

  /**
   * yOffset
   */
  int yOffset;

  /**
   * Creates a <code>DebugGraphics</code> object.
   */
  public DebugGraphics()
  {
    counter++;
  }

  /**
   * Creates a <code>DebugGraphics</code> object.
   *
   * @param graphics The <code>Graphics</code> object to wrap
   * @param component TODO
   */
  public DebugGraphics(Graphics graphics, JComponent component)
  {
    this(graphics);
    // FIXME: What shall we do with component ?
  }

  /**
   * Creates a <code>DebugGraphics</code> object.
   *
   * @param graphics The <code>Graphics</code> object to wrap
   */
  public DebugGraphics(Graphics graphics)
  {
    this();
    this.graphics = graphics;
  }

  /**
   * Sets the color to draw stuff with.
   * 
   * @param color The color
   */
  public void setColor(Color color)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      logStream().println(prefix() + " Setting color: " + color);

    graphics.setColor(color);
  }

  /**
   * Creates a overrides <code>Graphics.create</code> to create a
   * <code>DebugGraphics</code> object.
   *
   * @return a new <code>DebugGraphics</code> object.
   */
  public Graphics create()
  {
    DebugGraphics copy = new DebugGraphics(graphics.create());
    copy.debugOptions = debugOptions;
    return copy;
  }

  /**
   * Creates a overrides <code>Graphics.create</code> to create a
   * <code>DebugGraphics</code> object.
   *
   * @param x the x coordinate
   * @param y the y coordinate
   * @param width the width
   * @param height the height
   *
   * @return a new <code>DebugGraphics</code> object.
   */
  public Graphics create(int x, int y, int width, int height)
  {
    DebugGraphics copy = new DebugGraphics(graphics.create(x, y, width,
                                                           height));
    copy.debugOptions = debugOptions;
    return copy;
  }

  /**
   * flashColor
   *
   * @return Color
   */
  public static Color flashColor()
  {
    return debugFlashColor;
  }

  /**
   * setFlashColor
   *
   * @param color the color to use for flashing
   */
  public static void setFlashColor(Color color)
  {
    debugFlashColor = color;
  }

  /**
   * flashTime
   *
   * @return The time in milliseconds
   */
  public static int flashTime()
  {
    return debugFlashTime;
  }

  /**
   * setFlashTime
   *
   * @param time The time in milliseconds
   */
  public static void setFlashTime(int time)
  {
    debugFlashTime = time;
  }

  /**
   * flashCount
   *
   * @return The number of flashes
   */
  public static int flashCount()
  {
    return debugFlashCount;
  }

  /**
   * setFlashCount
   *
   * @param count The number of flashes
   */
  public static void setFlashCount(int count)
  {
    debugFlashCount = count;
  }

  /**
   * logStream
   *
   * @return The <code>PrintStream</code> to write logging messages to
   */
  public static PrintStream logStream()
  {
    return debugLogStream;
  }

  /**
   * setLogStream
   *
   * @param stream The currently set <code>PrintStream</code>.
   */
  public static void setLogStream(PrintStream stream)
  {
    debugLogStream = stream;
  }

  /**
   * getFont
   *
   * @return The font
   */
  public Font getFont()
  {
    return graphics.getFont();
  }

  /**
   * setFont
   *
   * @param font The font to use for drawing text
   */
  public void setFont(Font font)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      logStream().println(prefix() + " Setting font: " + font);

    graphics.setFont(font);
  }

  /**
   * Returns the color used for drawing.
   * 
   * @return The color.
   */
  public Color getColor()
  {
    return graphics.getColor();
  }

  /**
   * Returns the font metrics of the current font.
   *
   * @return a <code>FontMetrics</code> object
   */
  public FontMetrics getFontMetrics()
  {
    return graphics.getFontMetrics();
  }

  /**
   * Returns the font metrics for a given font.
   *
   * @param font the font to get the metrics for
   *
   * @return a <code>FontMetrics</code> object
   */
  public FontMetrics getFontMetrics(Font font)
  {
    return graphics.getFontMetrics(font);
  }

  /**
   * translate
   *
   * @param x the x coordinate
   * @param y the y coordinate
   */
  public void translate(int x, int y)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      logStream().println(prefix() + " Translating by: " + new Point(x, y));

    graphics.translate(x, y);
  }

  /**
   * setPaintMode
   */
  public void setPaintMode()
  {
    if ((debugOptions & LOG_OPTION) != 0)
      logStream().println(prefix() + " Setting paint mode");

    graphics.setPaintMode();
  }

  /**
   * setXORMode
   *
   * @param color the color
   */
  public void setXORMode(Color color)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      logStream().println(prefix() + " Setting XOR mode: " + color);

    graphics.setXORMode(color);
  }

  /**
   * getClipBounds
   *
   * @return Rectangle
   */
  public Rectangle getClipBounds()
  {
    return graphics.getClipBounds();
  }

  /**
   * Intersects the current clip region with the given region.
   *
   * @param x The x-position of the region
   * @param y The y-position of the region
   * @param width The width of the region
   * @param height The height of the region
   */
  public void clipRect(int x, int y, int width, int height)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().print(prefix() + " Setting clipRect: "
                          + new Rectangle(x, y, width, height));
      }

    graphics.clipRect(x, y, width, height);

    if ((debugOptions & LOG_OPTION) != 0)
      logStream().println(" Netting clipRect: " + graphics.getClipBounds());
  }

  /**
   * Sets the clipping region.
   *
   * @param x The x-position of the region
   * @param y The y-position of the region
   * @param width The width of the region
   * @param height The height of the region
   */
  public void setClip(int x, int y, int width, int height)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Setting new clipRect: "
                            + new Rectangle(x, y, width, height));
      }

    graphics.setClip(x, y, width, height);
  }

  /**
   * Returns the current clipping region.
   *
   * @return Shape
   */
  public Shape getClip()
  {
    return graphics.getClip();
  }

  /**
   * Sets the current clipping region
   *
   * @param shape The clippin region
   */
  public void setClip(Shape shape)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      logStream().println(prefix() + " Setting new clipRect: " + shape);

    graphics.setClip(shape);
  }

  private void sleep(int milliseconds)
  {
    try
      {
        Thread.sleep(milliseconds);
      }
    catch (InterruptedException e)
      {
        // Ignore this.
      }
  }
  
  /**
   * Draws a rectangle.
   *
   * @param x The x-position of the rectangle
   * @param y The y-position of the rectangle
   * @param width The width of the rectangle
   * @param height The height of the rectangle
   */
  public void drawRect(int x, int y, int width, int height)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing rect: "
                            + new Rectangle(x, y, width, height));
      }

    if ((debugOptions & FLASH_OPTION) != 0)
      {
        Color color = graphics.getColor();
        for (int index = 0; index < (debugFlashCount - 1); ++index)
          {
            graphics.setColor(color);
            graphics.drawRect(x, y, width, height);
            sleep(debugFlashTime);
            graphics.setColor(debugFlashColor);
            graphics.drawRect(x, y, width, height);
            sleep(debugFlashTime);
          }
        graphics.setColor(color);
      }

    graphics.drawRect(x, y, width, height);
  }

  /**
   * Draws a filled rectangle.
   *
   * @param x The x-position of the rectangle
   * @param y The y-position of the rectangle
   * @param width The width of the rectangle
   * @param height The height of the rectangle
   */
  public void fillRect(int x, int y, int width, int height)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Filling rect: "
                            + new Rectangle(x, y, width, height));
      }

    if ((debugOptions & FLASH_OPTION) != 0)
      {
        Color color = graphics.getColor();
        for (int index = 0; index < (debugFlashCount - 1); ++index)
          {
            graphics.setColor(color);
            graphics.fillRect(x, y, width, height);
            sleep(debugFlashTime);
            graphics.setColor(debugFlashColor);
            graphics.fillRect(x, y, width, height);
            sleep(debugFlashTime);
          }
        graphics.setColor(color);
      }

    graphics.fillRect(x, y, width, height);
  }

  /**
   * clearRect
   *
   * @param x The x-position of the rectangle
   * @param y The y-position of the rectangle
   * @param width The width of the rectangle
   * @param height The height of the rectangle
   */
  public void clearRect(int x, int y, int width, int height)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Clearing rect: "
                            + new Rectangle(x, y, width, height));
      }

    graphics.clearRect(x, y, width, height);
  }

  /**
   * drawRoundRect
   *
   * @param x The x-position of the rectangle
   * @param y The y-position of the rectangle
   * @param width The width of the rectangle
   * @param height The height of the rectangle
   * @param arcWidth TODO
   * @param arcHeight TODO
   */
  public void drawRoundRect(int x, int y, int width, int height, 
			    int arcWidth, int arcHeight)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing round rect: "
                            + new Rectangle(x, y, width, height)
                            + " arcWidth: " + arcWidth
                            + " arcHeight: " + arcHeight);
      }

    graphics.drawRoundRect(x, y, width, height, arcWidth, arcHeight);
  }

  /**
   * fillRoundRect
   *
   * @param x The x-position of the rectangle
   * @param y The y-position of the rectangle
   * @param width The width of the rectangle
   * @param height The height of the rectangle
   * @param arcWidth TODO
   * @param arcHeight TODO
   */
  public void fillRoundRect(int x, int y, int width, int height, 
			    int arcWidth, int arcHeight)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Filling round rect: "
                            + new Rectangle(x, y, width, height)
                            + " arcWidth: " + arcWidth
                            + " arcHeight: " + arcHeight);
      }

    graphics.fillRoundRect(x, y, width, height, arcWidth, arcHeight);
  }

  /**
   * drawLine
   *
   * @param x1 The x-position of the start 
   * @param y1 The y-position of the start
   * @param x2 The x-position of the end
   * @param y2 The y-position of the end
   */
  public void drawLine(int x1, int y1, int x2, int y2)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing line: from (" + x1 + ", "
                            + y1 + ") to (" + x2 + ", " + y2 + ")");
      }

    graphics.drawLine(x1, y1, x2, y2);
  }

  /**
   * draw3DRect
   *
   * @param x The x-position of the rectangle
   * @param y The y-position of the rectangle
   * @param width The width of the rectangle
   * @param height The height of the rectangle
   * @param raised TODO
   */
  public void draw3DRect(int x, int y, int width, int height, boolean raised)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing 3D rect: "
                            + new Rectangle(x, y, width, height)
                            + "Raised bezel: " + raised);
      }

    graphics.draw3DRect(x, y, width, height, raised);
  }

  /**
   * fill3DRect
   *
   * @param x The x-position of the rectangle
   * @param y The y-position of the rectangle
   * @param width The width of the rectangle
   * @param height The height of the rectangle
   * @param raised TODO
   */
  public void fill3DRect(int x, int y, int width, int height, boolean raised)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Filling 3D rect: "
                            + new Rectangle(x, y, width, height)
                            + "Raised bezel: " + raised);
      }

    graphics.fill3DRect(x, y, width, height, raised);
  }

  /**
   * drawOval
   *
   * @param x the x coordinate
   * @param y the y coordiante
   * @param width the width
   * @param height the height
   */
  public void drawOval(int x, int y, int width, int height)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing oval: "
                            + new Rectangle(x, y, width, height));
      }

    graphics.drawOval(x, y, width, height);
  }

  /**
   * fillOval
   *
   * @param x the x coordinate
   * @param y the y coordinate
   * @param width the width
   * @param height the height
   */
  public void fillOval(int x, int y, int width, int height)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Filling oval: "
                            + new Rectangle(x, y, width, height));
      }

    graphics.fillOval(x, y, width, height);
  }

  /**
   * drawArc
   *
   * @param x the x coordinate
   * @param y the y coordinate
   * @param width the width
   * @param height the height
   * @param startAngle TODO
   * @param arcAngle TODO
   */
  public void drawArc(int x, int y, int width, int height, 
		      int startAngle, int arcAngle)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing arc: "
                            + new Rectangle(x, y, width, height)
                            + " startAngle: " + startAngle
                            + " arcAngle: " + arcAngle);
      }

    graphics.drawArc(x, y, width, height, startAngle, arcAngle);
  }

  /**
   * fillArc
   *
   * @param x the coordinate
   * @param y the y coordinate
   * @param width the width
   * @param height the height
   * @param startAngle TODO
   * @param arcAngle TODO
   */
  public void fillArc(int x, int y, int width, int height, 
		      int startAngle, int arcAngle)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Filling arc: "
                            + new Rectangle(x, y, width, height)
                            + " startAngle: " + startAngle
                            + " arcAngle: " + arcAngle);
      }

    graphics.fillArc(x, y, width, height, startAngle, arcAngle);
  }

  /**
   * drawPolyline
   *
   * @param xpoints TODO
   * @param ypoints TODO
   * @param npoints TODO
   */
  public void drawPolyline(int[] xpoints, int[] ypoints, int npoints)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing polyline: nPoints: " + npoints
                            + " X's: " + xpoints + " Y's: " + ypoints);
      }

    graphics.drawPolyline(xpoints, ypoints, npoints);
  }

  /**
   * drawPolygon
   *
   * @param xpoints TODO
   * @param ypoints TODO
   * @param npoints TODO
   */
  public void drawPolygon(int[] xpoints, int[] ypoints, int npoints)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing polygon: nPoints: " + npoints
                            + " X's: " + xpoints + " Y's: " + ypoints);
      }

    graphics.drawPolygon(xpoints, ypoints, npoints);
  }

  /**
   * fillPolygon
   *
   * @param xpoints TODO
   * @param ypoints TODO
   * @param npoints TODO
   */
  public void fillPolygon(int[] xpoints, int[] ypoints, int npoints)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing polygon: nPoints: " + npoints
                            + " X's: " + xpoints + " Y's: " + ypoints);
      }

    graphics.fillPolygon(xpoints, ypoints, npoints);
  }

  /**
   * drawString
   *
   * @param string the string
   * @param x the x coordinate
   * @param y the y coordinate
   */
  public void drawString(String string, int x, int y)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing string: \"" + string
                            + "\" at: " + new Point(x, y));
      }

    graphics.drawString(string, x, y);
  }

  /**
   * drawString
   *
   * @param iterator TODO
   * @param x the x coordinate
   * @param y the y coordinate
   */
  public void drawString(AttributedCharacterIterator iterator,
			 int x, int y)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing string: \"" + iterator
                            + "\" at: " + new Point(x, y));
      }

    graphics.drawString(iterator, x, y);
  }

  /**
   * drawBytes
   * 
   * @param data TODO
   * @param offset TODO
   * @param length TODO
   * @param x the x coordinate
   * @param y the y coordinate
   */
  public void drawBytes(byte[] data, int offset, int length,
			int x, int y)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      logStream().println(prefix() + " Drawing bytes at: " + new Point(x, y));

    graphics.drawBytes(data, offset, length, x, y);
  }

  /**
   * drawChars
   * 
   * @param data array of characters to draw
   * @param offset offset in array
   * @param length number of characters in array to draw
   * @param x x-position
   * @param y y-position
   */
  public void drawChars(char[] data, int offset, int length, 
			int x, int y)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      logStream().println(prefix() + " Drawing chars at: " + new Point(x, y));

    if ((debugOptions & FLASH_OPTION) != 0)
      {
        Color color = graphics.getColor();
        for (int index = 0; index < (debugFlashCount - 1); ++index)
          {
            graphics.setColor(color);
            graphics.drawChars(data, offset, length, x, y);
            sleep(debugFlashTime);
            graphics.setColor(debugFlashColor);
            graphics.drawChars(data, offset, length, x, y);
            sleep(debugFlashTime);
          }
        graphics.setColor(color);
      }

    graphics.drawChars(data, offset, length, x, y);
  }

  /**
   * drawImage
   *
   * @param image The image to draw
   * @param x The x position
   * @param y The y position
   * @param observer The image observer
   * @return boolean
   */
  public boolean drawImage(Image image, int x, int y,
			   ImageObserver observer)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing image: " + image + " at: "
                          + new Point(x, y));
      }

    return graphics.drawImage(image, x, y, observer);
  }

  /**
   * drawImage
   * 
   * @param image The image to draw
   * @param x The x position
   * @param y The y position
   * @param width The width of the area to draw the image
   * @param height The height of the area to draw the image
   * @param observer The image observer
   *
   * @return boolean
   */
  public boolean drawImage(Image image, int x, int y, int width, 
			   int height, ImageObserver observer)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing image: " + image
                            + " at: " + new Rectangle(x, y, width, height));
      }

    return graphics.drawImage(image, x, y, width, height, observer);
  }

  /**
   * drawImage
   * 
   * @param image The image to draw
   * @param x The x position
   * @param y The y position
   * @param background The color for the background in the opaque regions
   * of the image
   * @param observer The image observer
   *
   * @return boolean
   */
  public boolean drawImage(Image image, int x, int y, 
			   Color background, ImageObserver observer)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing image: " + image
                            + " at: " + new Point(x, y)
                            + ", bgcolor: " + background);
      }

    return graphics.drawImage(image, x, y, background, observer);
  }

  /**
   * drawImage
   * 
   * @param image The image to draw
   * @param x The x position
   * @param y The y position
   * @param width The width of the area to draw the image
   * @param height The height of the area to draw the image
   * @param background The color for the background in the opaque regions
   * of the image
   * @param observer The image observer
   *
   * @return boolean
   */
  public boolean drawImage(Image image, int x, int y, int width, int height, 
			   Color background, ImageObserver observer)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing image: " + image
                            + " at: " + new Rectangle(x, y, width, height)
                            + ", bgcolor: " + background);
      }

    return graphics.drawImage(image, x, y, width, height, background, observer);
  }

  /**
   * drawImage
   * 
   * @param image The image to draw
   * @param dx1 TODO
   * @param dy1 TODO
   * @param dx2 TODO
   * @param dy2 TODO
   * @param sx1 TODO
   * @param sy1 TODO
   * @param sx2 TODO
   * @param sy2 TODO
   * @param observer The image observer
   * 
   * @return boolean
   */
  public boolean drawImage(Image image, int dx1, int dy1,
			   int dx2, int dy2, int sx1, int sy1, int sx2, int sy2,
			   ImageObserver observer)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing image: " + image
                         + " destination: " + new Rectangle(dx1, dy1, dx2, dy2)
                         + " source: " + new Rectangle(sx1, sy1, sx2, sy2));
      }

    return graphics.drawImage(image, dx1, dy1, dx2, dy2, sx1, sy1, sx2, sy2, observer);
  }

  /**
   * drawImage
   *
   * @param image The image to draw
   * @param dx1 TODO
   * @param dy1 TODO
   * @param dx2 TODO
   * @param dy2 TODO
   * @param sx1 TODO
   * @param sy1 TODO
   * @param sx2 TODO
   * @param sy2 TODO
   * @param background The color for the background in the opaque regions
   * of the image
   * @param observer The image observer
   *
   * @return boolean
   */
  public boolean drawImage(Image image, int dx1, int dy1,
			   int dx2, int dy2, int sx1, int sy1, int sx2, int sy2,
			   Color background, ImageObserver observer)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Drawing image: " + image
                         + " destination: " + new Rectangle(dx1, dy1, dx2, dy2)
                         + " source: " + new Rectangle(sx1, sy1, sx2, sy2)
                         + ", bgcolor: " + background);
      }

    return graphics.drawImage(image, dx1, dy1, dx2, dy2, sx1, sy1, sx2, sy2, background, observer);
  }

  /**
   * copyArea
   *
   * @param x The x position of the source area
   * @param y The y position of the source area
   * @param width The width of the area
   * @param height The height of the area
   * @param destx The x position of the destination area
   * @param desty The y posiiton of the destination area
   */
  public void copyArea(int x, int y, int width, int height, 
		       int destx, int desty)
  {
    if ((debugOptions & LOG_OPTION) != 0)
      {
        logStream().println(prefix() + " Copying area from: "
                            +  new Rectangle(x, y, width, height)
                            + " to: " + new Point(destx, desty));
      }

    graphics.copyArea(x, y, width, height, destx, desty);
  }

  /**
   * Releases all system resources that this <code>Graphics</code> is using.
   */
  public void dispose()
  {
    graphics.dispose();
    graphics = null;
  }

  /**
   * isDrawingBuffer
   *
   * @return boolean
   */
  public boolean isDrawingBuffer()
  {
    return false; // TODO
  }

  /**
   * setDebugOptions
   *
   * @param options the debug options
   */
  public void setDebugOptions(int options)
  {
    debugOptions = options;
    if ((debugOptions & LOG_OPTION) != 0)
      if (options == NONE_OPTION)
        logStream().println(prefix() + "Disabling debug");
      else
        logStream().println(prefix() + "Enabling debug");
  }

  /**
   * getDebugOptions
   *
   * @return the debug options
   */
  public int getDebugOptions()
  {
    return debugOptions;
  }

  /**
   * Creates and returns the prefix that should be prepended to all logging
   * messages. The prefix is made up like this:
   * 
   * <code>Graphics(<counter>-1)</code> where counter is an integer number
   * saying how many DebugGraphics objects have been created so far. The second
   * number always seem to be 1 on Sun's JDK, this has to be investigated a
   * little more.
   *
   * @return the prefix that should be prepended to all logging
   *         messages
   */
  private String prefix()
  {
    return "Graphics(" + counter + "-1)";
  }
}
