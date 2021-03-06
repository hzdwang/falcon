/*
 * Copyright (c) 2019 Robert Falkenberg.
 *
 * This file is part of FALCON
 * (see https://github.com/falkenber9/falcon).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 */
#include "spectrum.h"
#include "settings.h"

#include <math.h>
#include <climits>
#include "QDebug"
#include "QWidget"
#include "QMouseEvent"

#include <iostream>

#define FPS_TO_DELTA_MS(x) (1000.0/(x))
#define MAXIMUM_LINE_COUNT 1000

Spectrum::Spectrum(QWidget *parent, Settings *glob_settings) :
  QOpenGLWidget(parent),
  textureHandles{0, 0},
  textureBuffer(new GLubyte[MAXIMUM_LINE_COUNT * SPECTROGRAM_LINE_WIDTH * 4]),
  textureUpdateNeeded(false),
  lastUpdate()
#if PRINT_FPS
,fps_timer(this)
#endif
{
  settings = glob_settings;
  nextLine = settings->glob_args.spectrum_args.spectrum_line_count - 1;

  lastUpdate.start();
#if PRINT_FPS
  dataChangeCount = 0;
  textureUpdateCount = 0;
  textureDrawCount = 0;
  connect(&fps_timer, SIGNAL(timeout()), this, SLOT(printFrameCount()));
  fps_timer.start(1000);
#endif
}

Spectrum::~Spectrum() {
  glDeleteTextures(SPECTROGRAM_NOF_TEXTURES, textureHandles);
  delete[] textureBuffer;
}

void Spectrum::initializeGL() {
  initializeTextureBuffer();
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
}

void Spectrum::resizeGL(int width, int height) {
  glViewport(0, 0, static_cast<GLint>(width), static_cast<GLint>(height));
}

void Spectrum::initializeTextureBuffer() {
  for(int i = 0; i < MAXIMUM_LINE_COUNT * settings->glob_args.spectrum_args.spectrum_line_width * 4; i++){
    textureBuffer[i] = 0; //all zero = black
  }
  loadTexture();
}

void Spectrum::intensityToRGB(float intensity, GLubyte *rgbaOut) {

  // intensity cutoff
  if(intensity > max_intensity) intensity = max_intensity;
  float n = intensity_factor * 4.0f *((intensity - min_intensity) / USHRT_MAX);

  // get the colors
  rgbaOut[0] = static_cast<GLubyte>(UCHAR_MAX * std::min(std::max(std::min(n - 1.5f, -n + 4.5f), 0.0f), 1.0f));
  rgbaOut[1] = static_cast<GLubyte>(UCHAR_MAX * std::min(std::max(std::min(n - 0.5f, -n + 3.5f), 0.0f), 1.0f));
  rgbaOut[2] = static_cast<GLubyte>(UCHAR_MAX * std::min(std::max(std::min(n + 0.5f, -n + 2.5f), 0.0f), 1.0f));
  rgbaOut[3] = 0xFF;  //Opaque, no transparency
}

void Spectrum::addLine(const uint16_t *data) {
  if(!paused){
#if PRINT_FPS
    dataChangeCount++;
#endif
    GLubyte *buffer = textureBuffer + nextLine * settings->glob_args.spectrum_args.spectrum_line_width * 4;
    for (int col = 0; col < settings->glob_args.spectrum_args.spectrum_line_width; ++col)
    {
      intensityToRGB(data[col], buffer);
      buffer += 4;
    }
    nextLine--;
    if(nextLine < 0) nextLine = settings->glob_args.spectrum_args.spectrum_line_count - 1;
    textureUpdateNeeded = true;
    if(lastUpdate.elapsed() > FPS_TO_DELTA_MS(settings->glob_args.gui_args.wf_fps)) {
      lastUpdate.restart();
      update();
    }
  }
}

void Spectrum::loadTexture() {
  if(textureUpdateNeeded) {
#if PRINT_FPS
    textureUpdateCount++;
#endif
    textureUpdateNeeded = false;
    glDeleteTextures(SPECTROGRAM_NOF_TEXTURES, textureHandles);
    glGenTextures(SPECTROGRAM_NOF_TEXTURES, textureHandles);
    if ((nextLine + scroll_offset) + settings->glob_args.spectrum_args.spectrum_line_shown <= settings->glob_args.spectrum_args.spectrum_line_count - 1){  // nextLine hits buffer linearly
      glBindTexture(GL_TEXTURE_2D, textureHandles[0]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D,
                   0,  GL_RGBA,
                   settings->glob_args.spectrum_args.spectrum_line_width, settings->glob_args.spectrum_args.spectrum_line_shown,
                   0, GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   textureBuffer + (nextLine + scroll_offset) * settings->glob_args.spectrum_args.spectrum_line_width * 4);
    }else{
      // Newer Buffer
      glBindTexture(GL_TEXTURE_2D, textureHandles[1]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D,
                   0,  GL_RGBA,
                   settings->glob_args.spectrum_args.spectrum_line_width, settings->glob_args.spectrum_args.spectrum_line_count - (nextLine + scroll_offset) - 1,
                   0, GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   textureBuffer + (nextLine + scroll_offset) * settings->glob_args.spectrum_args.spectrum_line_width * 4);
      // Older Buffer
      glBindTexture(GL_TEXTURE_2D, textureHandles[0]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D,
                   0,  GL_RGBA,
                   settings->glob_args.spectrum_args.spectrum_line_width, settings->glob_args.spectrum_args.spectrum_line_shown - (settings->glob_args.spectrum_args.spectrum_line_count - (nextLine + scroll_offset) - 1) ,
                   0, GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   textureBuffer);
    }
  }
}

void Spectrum::drawSpectrogram() {
#if PRINT_FPS
  textureDrawCount++;
#endif

  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_TEXTURE_2D);

  if((nextLine + scroll_offset) + settings->glob_args.spectrum_args.spectrum_line_shown <= settings->glob_args.spectrum_args.spectrum_line_count - 1) {
    // Draw newer buffer
    float startX = 0.0;
    float startY = 0.0;
    float endX = 1.0;
    float endY = 1.0;
    glBindTexture(GL_TEXTURE_2D, textureHandles[0]);
    glBegin(GL_QUADS);
    glTexCoord2d(0.0, 1.0); glVertex3f(startX, startY, 0.0);
    glTexCoord2d(1.0, 1.0); glVertex3f(endX, startY, 0.0);
    glTexCoord2d(1.0, 0.0); glVertex3f(endX, endY, 0.0);
    glTexCoord2d(0.0, 0.0); glVertex3f(startX, endY, 0.0);
    glEnd();
  }
  else{ // More lines to see than available
    // Draw newer buffer
    float startX = 0.0;
    float startY = 0.0;
    float endX = 1.0;
    float endY = static_cast<float>(settings->glob_args.spectrum_args.spectrum_line_shown - (settings->glob_args.spectrum_args.spectrum_line_count - (nextLine + scroll_offset) - 1)) / settings->glob_args.spectrum_args.spectrum_line_shown;  // What is the (percentage) of waterfall that can be filled with the available lines?
    glBindTexture(GL_TEXTURE_2D, textureHandles[0]);
    glBegin(GL_QUADS);
    glTexCoord2d(0.0, 1.0); glVertex3f(startX, startY, 0.0);
    glTexCoord2d(1.0, 1.0); glVertex3f(endX, startY, 0.0);
    glTexCoord2d(1.0, 0.0); glVertex3f(endX, endY, 0.0);
    glTexCoord2d(0.0, 0.0); glVertex3f(startX, endY, 0.0);
    glEnd();

    // Draw older buffer to fill rest of waterfall area
    startY = endY;
    endY = 1.0;
    glBindTexture(GL_TEXTURE_2D, textureHandles[1]);
    glBegin(GL_QUADS);
    glTexCoord2d(0.0, 1.0); glVertex3f(startX, startY, 0.0);
    glTexCoord2d(1.0, 1.0); glVertex3f(endX, startY, 0.0);
    glTexCoord2d(1.0, 0.0); glVertex3f(endX, endY, 0.0);
    glTexCoord2d(0.0, 0.0); glVertex3f(startX, endY, 0.0);
    glEnd();
  }

  glDisable(GL_TEXTURE_2D);
  glFlush();
}

void Spectrum::paintGL() {
  loadTexture();
  drawSpectrogram();
}

void Spectrum::mousePressEvent(QMouseEvent * event) {
  (void)event;
  emit subwindow_click();
}

void Spectrum::scroll_up() {
  if (scroll_offset + settings->glob_args.spectrum_args.mouse_wheel_sens <= settings->glob_args.spectrum_args.spectrum_line_count - settings->glob_args.spectrum_args.spectrum_line_shown - 1){
      scroll_offset += settings->glob_args.spectrum_args.mouse_wheel_sens;
  textureUpdateNeeded = true;
  update();
  } else if (scroll_offset < settings->glob_args.spectrum_args.spectrum_line_count - settings->glob_args.spectrum_args.spectrum_line_shown - 1){
scroll_offset = settings->glob_args.spectrum_args.spectrum_line_count - settings->glob_args.spectrum_args.spectrum_line_shown - 1;
textureUpdateNeeded = true;
update();
  }else{
      return;
  }
}

void Spectrum::scroll_down() {
    if (scroll_offset - settings->glob_args.spectrum_args.mouse_wheel_sens >= 0){
        scroll_offset -= settings->glob_args.spectrum_args.mouse_wheel_sens;
        textureUpdateNeeded = true;
        update();
    }else if(scroll_offset > 0){
        scroll_offset = 0;
        textureUpdateNeeded = true;
        update();
    }else{return;}

}

#if PRINT_FPS
void Spectrum::printFrameCount() {
  std::cout << "FPS (dataChanges, textureUpdates, textureRedraws): " << dataChangeCount << ", " << textureUpdateCount << ", " << textureDrawCount << std::endl;
  dataChangeCount = 0;
  textureUpdateCount = 0;
  textureDrawCount = 0;
}
#endif
