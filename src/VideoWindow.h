// Copyright � 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#pragma once

#include "OpenGLWindow.h"

#include <QOpenGLShaderProgram>

namespace OrientView
{
	class VideoWindow : public OpenGLWindow
	{

	public:

		explicit VideoWindow(QWindow* parent = 0);
		~VideoWindow();

		void initialize();
		void render();

	private:

		GLuint posAttr = 0;
		GLuint colAttr = 0;
		GLuint matrixUniform = 0;

		QOpenGLShaderProgram* program = nullptr;
	};
}
