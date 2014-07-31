// Copyright � 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#pragma once

#include <QMainWindow>

namespace Ui
{
	class MainWindow;
}

namespace OrientView
{
	class VideoWindow;
	class EncodeWindow;
	class Settings;
	class VideoDecoder;
	class QuickRouteJpegReader;
	class VideoStabilizer;
	class VideoRenderer;
	class VideoEncoder;
	class VideoDecoderThread;
	class RenderOnScreenThread;
	class RenderOffScreenThread;
	class VideoEncoderThread;

	class MainWindow : public QMainWindow
	{
		Q_OBJECT

	public:

		explicit MainWindow(QWidget *parent = 0);
		~MainWindow();

	private slots:

		void on_actionOpen_triggered();
		void on_actionSaveAs_triggered();
		void on_actionPlayVideo_triggered();
		void on_actionEncodeVideo_triggered();
		void on_actionExit_triggered();

		void on_pushButtonBrowseVideoFile_clicked();
		void on_pushButtonBrowseMapFile_clicked();
		void on_pushButtonBrowseGpxFile_clicked();
		void on_pushButtonBrowseOutputFile_clicked();
		void on_pushButtonLoadCalibrationData_clicked();

		void videoWindowClosing();
		void encodeWindowClosing();

	private:

		void readSettings();
		void writeSettings();

		void closeEvent(QCloseEvent* event);

		Ui::MainWindow* ui = nullptr;
		VideoWindow* videoWindow = nullptr;
		EncodeWindow* encodeWindow = nullptr;
		Settings* settings = nullptr;
		VideoDecoder* videoDecoder = nullptr;
		QuickRouteJpegReader* quickRouteJpegReader = nullptr;
		VideoStabilizer* videoStabilizer = nullptr;
		VideoRenderer* videoRenderer = nullptr;
		VideoEncoder* videoEncoder = nullptr;
		VideoDecoderThread* videoDecoderThread = nullptr;
		RenderOnScreenThread* renderOnScreenThread = nullptr;
		RenderOffScreenThread* renderOffScreenThread = nullptr;
		VideoEncoderThread* videoEncoderThread = nullptr;
	};
}
