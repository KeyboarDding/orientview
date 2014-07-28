// Copyright � 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#pragma once

#include <QString>

namespace OrientView
{
	class Settings
	{

	public:

		Settings();

		bool initialize(const QString& fileName);
		void shutdown();

		struct Display
		{
			int width = 0;
			int height = 0;
			int multisamples = 0;
			bool fullscreen;
			bool hideCursor;

		} display;

		struct Shaders
		{
			QString vertexShaderPath;
			QString fragmentShaderPath;

		} shaders;

		struct Appearance
		{
			float mapPanelWidth = 0.0f;
			bool showInfoPanel;

		} appearance;

		struct Decoder
		{
			int frameCountDivisor = 0;
			int frameDurationDivisor = 0;

		} decoder;

		struct Encoder
		{
			QString preset;
			QString profile;
			int constantRateFactor = 0;

		} encoder;
	};
}
