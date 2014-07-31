// Copyright � 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#include <QElapsedTimer>
#include <QOpenGLFramebufferObjectFormat>

#include "RenderOffScreenThread.h"
#include "MainWindow.h"
#include "EncodeWindow.h"
#include "VideoDecoderThread.h"
#include "VideoStabilizer.h"
#include "VideoRenderer.h"
#include "Settings.h"
#include "FrameData.h"

using namespace OrientView;

RenderOffScreenThread::RenderOffScreenThread()
{
}

bool RenderOffScreenThread::initialize(MainWindow* mainWindow, EncodeWindow* encodeWindow, VideoDecoderThread* videoDecoderThread, VideoStabilizer* videoStabilizer, VideoRenderer* videoRenderer, Settings* settings)
{
	qDebug("Initializing RenderOffScreenThread");

	this->mainWindow = mainWindow;
	this->encodeWindow = encodeWindow;
	this->videoDecoderThread = videoDecoderThread;
	this->videoStabilizer = videoStabilizer;
	this->videoRenderer = videoRenderer;

	framebufferWidth = settings->window.width;
	framebufferHeight = settings->window.height;
	stabilizationEnabled = settings->stabilization.enabled;

	QOpenGLFramebufferObjectFormat mainFboFormat;
	mainFboFormat.setSamples(settings->window.multisamples);
	mainFboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);

	mainFramebuffer = new QOpenGLFramebufferObject(framebufferWidth, framebufferHeight, mainFboFormat);

	if (!mainFramebuffer->isValid())
	{
		qWarning("Could not create main frame buffer");
		return false;
	}

	QOpenGLFramebufferObjectFormat secondaryFboFormat;
	secondaryFboFormat.setSamples(0);
	secondaryFboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);

	secondaryFramebuffer = new QOpenGLFramebufferObject(framebufferWidth, framebufferHeight, secondaryFboFormat);

	if (!secondaryFramebuffer->isValid())
	{
		qWarning("Could not create secondary frame buffer");
		return false;
	}

	renderedFrameData = FrameData();
	renderedFrameData.dataLength = framebufferWidth * framebufferHeight * 4;
	renderedFrameData.rowLength = framebufferWidth * 4;
	renderedFrameData.data = new uint8_t[(size_t)renderedFrameData.dataLength];
	renderedFrameData.width = framebufferWidth;
	renderedFrameData.height = framebufferHeight;

	frameReadSemaphore = new QSemaphore();
	frameAvailableSemaphore = new QSemaphore();

	return true;
}

void RenderOffScreenThread::shutdown()
{
	qDebug("Shutting down RenderOffScreenThread");

	if (frameAvailableSemaphore != nullptr)
	{
		delete frameAvailableSemaphore;
		frameAvailableSemaphore = nullptr;
	}

	if (frameReadSemaphore != nullptr)
	{
		delete frameReadSemaphore;
		frameReadSemaphore = nullptr;
	}

	if (renderedFrameData.data)
	{
		delete renderedFrameData.data;
		renderedFrameData.data = nullptr;
	}

	if (secondaryFramebuffer != nullptr)
	{
		delete secondaryFramebuffer;
		secondaryFramebuffer = nullptr;
	}

	if (mainFramebuffer != nullptr)
	{
		delete mainFramebuffer;
		mainFramebuffer = nullptr;
	}
}

void RenderOffScreenThread::run()
{
	FrameData decodedFrameData;
	FrameData decodedFrameDataGrayscale;
	
	QElapsedTimer frameDurationTimer;
	double frameDuration = 0.0;

	frameDurationTimer.start();
	frameReadSemaphore->release(1);

	while (!isInterruptionRequested())
	{
		if (videoDecoderThread->tryGetNextFrame(&decodedFrameData, &decodedFrameDataGrayscale))
		{
			if (stabilizationEnabled)
				videoStabilizer->processFrame(&decodedFrameDataGrayscale);

			encodeWindow->getContext()->makeCurrent(encodeWindow->getSurface());
			mainFramebuffer->bind();

			videoRenderer->startRendering(framebufferWidth, framebufferHeight, frameDuration);
			videoRenderer->uploadFrameData(&decodedFrameData);
			videoDecoderThread->signalFrameRead();
			videoRenderer->renderVideoPanel();
			videoRenderer->renderMapPanel();
			videoRenderer->renderInfoPanel(0);
			videoRenderer->stopRendering();

			while (!frameReadSemaphore->tryAcquire(1, 20) && !isInterruptionRequested()) {}

			if (isInterruptionRequested())
				break;

			readDataFromFramebuffer(mainFramebuffer);
			renderedFrameData.duration = decodedFrameData.duration;
			renderedFrameData.number = decodedFrameData.number;

			frameAvailableSemaphore->release(1);

			frameDuration = frameDurationTimer.nsecsElapsed() / 1000000.0;
			frameDurationTimer.restart();
		}
	}

	encodeWindow->getContext()->doneCurrent();
	encodeWindow->getContext()->moveToThread(mainWindow->thread());
}

bool RenderOffScreenThread::tryGetNextFrame(FrameData* frameData)
{
	if (frameAvailableSemaphore->tryAcquire(1, 20))
	{
		frameData->data = renderedFrameData.data;
		frameData->dataLength = renderedFrameData.dataLength;
		frameData->rowLength = renderedFrameData.rowLength;
		frameData->width = renderedFrameData.width;
		frameData->height = renderedFrameData.height;
		frameData->duration = renderedFrameData.duration;
		frameData->number = renderedFrameData.number;

		return true;
	}
	else
		return false;
}

void RenderOffScreenThread::signalFrameRead()
{
	frameReadSemaphore->release(1);
}

void RenderOffScreenThread::readDataFromFramebuffer(QOpenGLFramebufferObject* sourceFbo)
{
	if (sourceFbo->format().samples() != 0)
	{
		QRect rect(0, 0, framebufferWidth, framebufferHeight);
		QOpenGLFramebufferObject::blitFramebuffer(secondaryFramebuffer, rect, sourceFbo, rect);
		readDataFromFramebuffer(secondaryFramebuffer);

		return;
	}

	sourceFbo->bind();
	glReadPixels(0, 0, framebufferWidth, framebufferHeight, GL_RGBA, GL_UNSIGNED_BYTE, renderedFrameData.data);
}
