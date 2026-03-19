#include "tapmodel.h"
#include <QRandomGenerator>
#include <QtMath>
#include <QtMath>
static qreal randRange(qreal minValue, qreal maxValue)
{
    return minValue + (maxValue - minValue) * QRandomGenerator::global()->generateDouble();
}
namespace TapModel {


    OffsetSeed makeOffsetSeed()
{
    OffsetSeed seed;
    seed.phaseX = QRandomGenerator::global()->bounded(6.28318530717958647692);
    seed.phaseY = QRandomGenerator::global()->bounded(6.28318530717958647692);

    // 初始主落点偏差，幅度很小
    seed.biasX = (QRandomGenerator::global()->bounded(2001) - 1000) / 100000.0; // about [-0.01, 0.01]
    seed.biasY = (QRandomGenerator::global()->bounded(2001) - 1000) / 100000.0;

    return seed;
}

QPointF offsetAt(qreal elapsedMs, const OffsetSeed &seed)
{
    // 包络：按下时逐渐形成，抬起时逐渐减弱
    const qreal pressure = pressureAt(elapsedMs);
    const qreal envelope = qBound<qreal>(0.0, pressure, 1.0);

    // 很小的慢漂移 + 更小的微抖
    const qreal tSec = elapsedMs / 1000.0;

    const qreal driftX =
        seed.biasX * envelope +
        0.0025 * envelope * qSin(2.0 * 3.14159265358979323846 * 7.0 * tSec + seed.phaseX);

    const qreal driftY =
        seed.biasY * envelope +
        0.0020 * envelope * qSin(2.0 * 3.14159265358979323846 * 9.0 * tSec + seed.phaseY);

    return QPointF(driftX, driftY);
}

qreal smoothStep(qreal x)
{
    x = qBound<qreal>(0.0, x, 1.0);
    return x * x * (3.0 - 2.0 * x);
}


}
qreal TapModel::pressureAt(qreal elapsedMs,
                           const PressureSeed &seed,
                           qreal onsetMs,
                           qreal riseMs,
                           qreal holdMs,
                           qreal fallMs,
                           qreal peak,
                           qreal jitterAmp,
                           qreal jitterHz)
{
    constexpr qreal kTwoPi = 6.28318530717958647692;

    const qreal finalOnsetMs  = qMax<qreal>(0.0, onsetMs + seed.onsetOffsetMs);
    const qreal finalRiseMs   = qMax<qreal>(1.0, riseMs * seed.riseScale);
    const qreal finalHoldMs   = qMax<qreal>(1.0, holdMs * seed.holdScale);
    const qreal finalFallMs   = qMax<qreal>(1.0, fallMs * seed.fallScale);
    const qreal finalPeak     = qBound<qreal>(0.0, peak * seed.peakScale, 1.0);
    const qreal finalJitterAmp = qMax<qreal>(0.0, jitterAmp * seed.jitterAmpScale);
    const qreal finalJitterHz  = qMax<qreal>(0.1, jitterHz * seed.jitterHzScale);

    const qreal riseStart = finalOnsetMs;
    const qreal riseEnd   = riseStart + finalRiseMs;
    const qreal holdEnd   = riseEnd + finalHoldMs;
    const qreal fallEnd   = holdEnd + finalFallMs;

    if (elapsedMs < riseStart) {
        return 0.0;
    }

    if (elapsedMs < riseEnd) {
        const qreal x = (elapsedMs - riseStart) / finalRiseMs;
        return qBound<qreal>(0.0, finalPeak * smoothStep(x), 1.0);
    }

    if (elapsedMs < holdEnd) {
        const qreal tSec = (elapsedMs - riseEnd) / 1000.0;
        const qreal tremor = 1.0 + finalJitterAmp * qSin(kTwoPi * finalJitterHz * tSec);
        return qBound<qreal>(0.0, finalPeak * tremor, 1.0);
    }

    if (elapsedMs < fallEnd) {
        const qreal x = (elapsedMs - holdEnd) / finalFallMs;
        return qBound<qreal>(0.0, finalPeak * (1.0 - smoothStep(x)), 1.0);
    }

    return 0.0;
}
TapModel::PressureSeed TapModel::makePressureSeed()
{
    PressureSeed seed;

    // 这些范围先保守一点，只做“轻微变化”
    seed.onsetOffsetMs   = randRange(-3.0, 3.0);
    seed.riseScale       = randRange(0.90, 1.12);
    seed.holdScale       = randRange(0.88, 1.18);
    seed.fallScale       = randRange(0.90, 1.15);
    seed.peakScale       = randRange(0.96, 1.04);
    seed.jitterAmpScale  = randRange(0.80, 1.25);
    seed.jitterHzScale   = randRange(0.90, 1.12);

    return seed;
}