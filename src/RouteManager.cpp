// Copyright © 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#define _USE_MATH_DEFINES
#include <cmath>

#include "RouteManager.h"
#include "QuickRouteReader.h"
#include "MapImageReader.h"
#include "Settings.h"

using namespace OrientView;

void RouteManager::initialize(QuickRouteReader* quickRouteReader, SplitTimeManager* splitTimeManager, MapImageReader* mapImageReader, Settings* settings)
{
	mapImageWidth = mapImageReader->getMapImage().width();
	mapImageHeight = mapImageReader->getMapImage().height();

	defaultRoute.routePoints = quickRouteReader->getRoutePoints();
	defaultRoute.splitTimes = splitTimeManager->getDefaultSplitTimes();
	defaultRoute.controlsTimeOffset = settings->route.controlsTimeOffset;
	defaultRoute.runnerTimeOffset = settings->route.runnerTimeOffset;
	defaultRoute.userScale = settings->route.scale;
	defaultRoute.highPace = settings->route.highPace;
	defaultRoute.lowPace = settings->route.lowPace;
	defaultRoute.wholeRouteRenderMode = settings->route.wholeRouteRenderMode;
	defaultRoute.showRunner = settings->route.showRunner;
	defaultRoute.showControls = settings->route.showControls;
	defaultRoute.wholeRouteColor = settings->route.wholeRouteColor;
	defaultRoute.wholeRouteWidth = settings->route.wholeRouteWidth;
	defaultRoute.controlBorderColor = settings->route.controlBorderColor;
	defaultRoute.controlRadius = settings->route.controlRadius;
	defaultRoute.controlBorderWidth = settings->route.controlBorderWidth;
	defaultRoute.runnerColor = settings->route.runnerColor;
	defaultRoute.runnerBorderColor = settings->route.runnerBorderColor;
	defaultRoute.runnerBorderWidth = settings->route.runnerBorderWidth;
	defaultRoute.runnerScale = settings->route.runnerScale;

	generateAlignedRoutePoints();
	constructWholeRoutePath();
	calculateRoutePointColors();

	fullUpdateRequested = true;
	update(0);
}

void RouteManager::update(double currentTime)
{
	if (fullUpdateRequested)
	{
		calculateControlPositions();
		calculateSplitTransformations();
		fullUpdateRequested = false;
	}

	calculateRunnerPosition(currentTime);
	calculateCurrentSplitTransformation(currentTime);
}

void RouteManager::requestFullUpdate()
{
	fullUpdateRequested = true;
}

double RouteManager::getX() const
{
	return defaultRoute.currentSplitTransformation.x;
}

double RouteManager::getY() const
{
	return defaultRoute.currentSplitTransformation.y;
}

double RouteManager::getScale() const
{
	return defaultRoute.currentSplitTransformation.scale;
}

double RouteManager::getAngle() const
{
	return defaultRoute.currentSplitTransformation.angle;
}

Route& RouteManager::getDefaultRoute()
{
	return defaultRoute;
}

void RouteManager::generateAlignedRoutePoints()
{
	if (defaultRoute.routePoints.size() < 2)
		return;

	double alignedTime = 0.0;
	RoutePoint currentRoutePoint = defaultRoute.routePoints.at(0);
	RoutePoint alignedRoutePoint;

	// align and interpolate route point data to one second intervals
	for (size_t i = 0; i < defaultRoute.routePoints.size() - 1;)
	{
		size_t nextIndex = 0;

		for (size_t j = i + 1; j < defaultRoute.routePoints.size(); ++j)
		{
			if (defaultRoute.routePoints.at(j).time - currentRoutePoint.time > 1.0)
			{
				nextIndex = j;
				break;
			}
		}

		if (nextIndex <= i)
			break;

		i = nextIndex;

		RoutePoint nextRoutePoint = defaultRoute.routePoints.at(nextIndex);

		alignedRoutePoint.dateTime = currentRoutePoint.dateTime;
		alignedRoutePoint.coordinate = currentRoutePoint.coordinate;

		double timeDelta = nextRoutePoint.time - currentRoutePoint.time;
		double alphaStep = 1.0 / timeDelta;
		double alpha = 0.0;
		int stepCount = (int)timeDelta;

		for (int k = 0; k <= stepCount; ++k)
		{
			alignedRoutePoint.time = alignedTime;
			alignedRoutePoint.position.setX((1.0 - alpha) * currentRoutePoint.position.x() + alpha * nextRoutePoint.position.x());
			alignedRoutePoint.position.setY((1.0 - alpha) * currentRoutePoint.position.y() + alpha * nextRoutePoint.position.y());
			alignedRoutePoint.elevation = (1.0 - alpha) * currentRoutePoint.elevation + alpha * nextRoutePoint.elevation;
			alignedRoutePoint.heartRate = (1.0 - alpha) * currentRoutePoint.heartRate + alpha * nextRoutePoint.heartRate;
			alignedRoutePoint.pace = (1.0 - alpha) * currentRoutePoint.pace + alpha * nextRoutePoint.pace;

			alpha += alphaStep;

			if (k < stepCount)
			{
				defaultRoute.alignedRoutePoints.push_back(alignedRoutePoint);
				alignedTime += 1.0;
			}
		}

		currentRoutePoint = alignedRoutePoint;
		currentRoutePoint.dateTime = nextRoutePoint.dateTime;
		currentRoutePoint.coordinate = nextRoutePoint.coordinate;
	}

	defaultRoute.alignedRoutePoints.push_back(alignedRoutePoint);
}

void RouteManager::constructWholeRoutePath()
{
	size_t routePointCount = defaultRoute.routePoints.size();

	if (routePointCount >= 2)
	{
		for (size_t i = 0; i < routePointCount; ++i)
		{
			RoutePoint rp = defaultRoute.routePoints.at(i);

			double x = rp.position.x();
			double y = rp.position.y();

			if (i == 0)
				defaultRoute.wholeRoutePath.moveTo(x, y);
			else
				defaultRoute.wholeRoutePath.lineTo(x, y);
		}
	}
}

void RouteManager::calculateControlPositions()
{
	defaultRoute.controlPositions.clear();

	for (size_t i = 0; i < defaultRoute.splitTimes.splitTimes.size(); ++i)
	{
		SplitTime splitTime = defaultRoute.splitTimes.splitTimes.at(i);

		double offsetTime = splitTime.time + defaultRoute.controlsTimeOffset;
		double previousWholeSecond = floor(offsetTime);
		double alpha = offsetTime - previousWholeSecond;

		int firstIndex = (int)previousWholeSecond;
		int secondIndex = firstIndex + 1;
		int indexMax = (int)defaultRoute.alignedRoutePoints.size() - 1;

		firstIndex = std::max(0, std::min(firstIndex, indexMax));
		secondIndex = std::max(0, std::min(secondIndex, indexMax));

		if (firstIndex == secondIndex)
			defaultRoute.controlPositions.push_back(defaultRoute.alignedRoutePoints.at(firstIndex).position);
		else
		{
			RoutePoint rp1 = defaultRoute.alignedRoutePoints.at(firstIndex);
			RoutePoint rp2 = defaultRoute.alignedRoutePoints.at(secondIndex);

			defaultRoute.controlPositions.push_back((1.0 - alpha) * rp1.position + alpha * rp2.position);
		}
	}
}

void RouteManager::calculateSplitTransformations()
{
	defaultRoute.splitTransformations.clear();

	for (size_t i = 0; i < defaultRoute.splitTimes.splitTimes.size() - 1; ++i)
	{
		SplitTime st1 = defaultRoute.splitTimes.splitTimes.at(i);
		SplitTime st2 = defaultRoute.splitTimes.splitTimes.at(i + 1);

		size_t startIndex = (size_t)round(st1.time + defaultRoute.controlsTimeOffset);
		size_t stopIndex = (size_t)round(st2.time + defaultRoute.controlsTimeOffset);
		size_t indexMax = defaultRoute.alignedRoutePoints.size() - 1;

		startIndex = std::max((size_t)0, std::min(startIndex, indexMax));
		stopIndex = std::max((size_t)0, std::min(stopIndex, indexMax));

		SplitTransformation splitTransformation;

		if (startIndex != stopIndex)
		{
			RoutePoint startRp = defaultRoute.alignedRoutePoints.at(startIndex);
			RoutePoint stopRp = defaultRoute.alignedRoutePoints.at(stopIndex);
			QPointF startToStop = stopRp.position - startRp.position;

			double angle = atan2(-startToStop.y(), startToStop.x());
			double finalAngle = 0.0;

			// always rotate towards positive y-axis
			if (angle >= 0.0 && angle < (M_PI / 2.0))
				finalAngle = (M_PI / 2.0) - angle;
			else if (angle >= (M_PI / 2.0))
				finalAngle = -(angle - (M_PI / 2.0));
			else if (angle < 0.0 && angle >= -(M_PI / 2.0))
				finalAngle = (M_PI / 2.0) + (-angle);
			else if (angle < -(M_PI / 2.0))
				finalAngle = -((M_PI + angle) + (M_PI / 2.0));

			finalAngle *= 180.0 / M_PI;

			QMatrix rotate;
			rotate.rotate(-finalAngle);

			double minX = std::numeric_limits<double>::max();
			double maxX = std::numeric_limits<double>::min();
			double minY = std::numeric_limits<double>::max();
			double maxY = std::numeric_limits<double>::min();

			for (size_t j = startIndex; j <= stopIndex; ++j)
			{
				QPointF position = rotate.map(defaultRoute.alignedRoutePoints.at(j).position);
				
				if (position.x() < minX)
					minX = position.x();

				if (position.x() > maxX)
					maxX = position.x();

				if (position.y() < minY)
					minY = position.y();

				if (position.y() > maxY)
					maxY = position.y();
			}

			double width = maxX - minX;
			double height = maxY - minY;
			double scaleX = mapImageWidth / width;
			double scaleY = mapImageHeight / height;
			double finalScale = scaleX < scaleY ? scaleX : scaleY;
			
			QPointF middlePoint = (startRp.position + stopRp.position) / 2.0;
		
			splitTransformation.x = -middlePoint.x();
			splitTransformation.y = middlePoint.y();
			splitTransformation.angle = finalAngle;
			//splitTransformation.scale = finalScale;
			splitTransformation.scale = 1.0;
		}

		defaultRoute.splitTransformations.push_back(splitTransformation);
	}
}

void RouteManager::calculateRoutePointColors()
{
	for (RoutePoint& rp : defaultRoute.routePoints)
		rp.color = interpolateFromGreenToRed(defaultRoute.highPace, defaultRoute.lowPace, rp.pace);

	for (RoutePoint& rp : defaultRoute.alignedRoutePoints)
		rp.color = interpolateFromGreenToRed(defaultRoute.highPace, defaultRoute.lowPace, rp.pace);
}

void RouteManager::calculateRunnerPosition(double currentTime)
{
	double offsetTime = currentTime + defaultRoute.runnerTimeOffset;
	double previousWholeSecond = floor(offsetTime);
	double alpha = offsetTime - previousWholeSecond;

	int firstIndex = (int)previousWholeSecond;
	int secondIndex = firstIndex + 1;
	int indexMax = (int)defaultRoute.alignedRoutePoints.size() - 1;

	firstIndex = std::max(0, std::min(firstIndex, indexMax));
	secondIndex = std::max(0, std::min(secondIndex, indexMax));

	if (firstIndex == secondIndex)
		defaultRoute.runnerPosition = defaultRoute.alignedRoutePoints.at(firstIndex).position;
	else
	{
		RoutePoint rp1 = defaultRoute.alignedRoutePoints.at(firstIndex);
		RoutePoint rp2 = defaultRoute.alignedRoutePoints.at(secondIndex);

		defaultRoute.runnerPosition = (1.0 - alpha) * rp1.position + alpha * rp2.position;
	}
}

void RouteManager::calculateCurrentSplitTransformation(double currentTime)
{
	for (size_t i = 0; i < defaultRoute.splitTimes.splitTimes.size() - 1; ++i)
	{
		SplitTime st1 = defaultRoute.splitTimes.splitTimes.at(i);
		SplitTime st2 = defaultRoute.splitTimes.splitTimes.at(i + 1);

		if (currentTime >= st1.time && currentTime < st2.time)
		{
			defaultRoute.currentSplitTransformation = defaultRoute.splitTransformations.at(i);
			break;
		}
	}
}

QColor RouteManager::interpolateFromGreenToRed(double greenValue, double redValue, double value)
{
	double alpha = (value - greenValue) / (redValue - greenValue);
	alpha = std::max(0.0, std::min(alpha, 1.0));

	double r = (alpha > 0.5 ? 1.0 : 2.0 * alpha);
	double g = (alpha > 0.5 ? 1.0 - 2.0 * (alpha - 0.5) : 1.0);
	double b = 0.0;

	return QColor::fromRgbF(r, g, b);
}
