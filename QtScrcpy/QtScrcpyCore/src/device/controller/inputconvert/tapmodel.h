#pragma once

#include <QPointF>
#include <QtGlobal>

namespace TapModel {

struct OffsetSeed {
    qreal phaseX = 0.0;
    qreal phaseY = 0.0;
    qreal biasX = 0.0;
    qreal biasY = 0.0;
};

struct PressureSeed {
    qreal onsetOffsetMs = 0.0;
    qreal riseScale = 1.0;
    qreal holdScale = 1.0;
    qreal fallScale = 1.0;
    qreal peakScale = 1.0;
    qreal jitterAmpScale = 1.0;
    qreal jitterHzScale = 1.0;
};

qreal smoothStep(qreal x);

PressureSeed makePressureSeed();

qreal pressureAt(qreal elapsedMs,
                 const PressureSeed &seed,
                 qreal onsetMs   = 18.0,
                 qreal riseMs    = 22.0,
                 qreal holdMs    = 38.0,
                 qreal fallMs    = 26.0,
                 qreal peak      = 0.92,
                 qreal jitterAmp = 0.018,
                 qreal jitterHz  = 28.0);

OffsetSeed makeOffsetSeed();

QPointF offsetAt(qreal elapsedMs, const OffsetSeed &seed);

}