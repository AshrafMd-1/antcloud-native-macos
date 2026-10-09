#include "bandwidth.h"
#include <QtTest>
using namespace std::chrono;

class BandwidthTest : public QObject {
    Q_OBJECT
    void add(BandwidthTracker& tracker, int startMs, size_t bytes) {
        auto& bucket = tracker.buckets[(startMs / tracker.bucketIntervalMs) % tracker.bucketCount];
        bucket.start = steady_clock::time_point(milliseconds(startMs));
        bucket.bytes = bytes;
    }
private slots:
    void constantRateDoesNotDependOnCurrentBucketPhase() {
        // Ten complete 250 ms samples at 1 Mbps must always report 1 Mbps,
        // regardless of how far the unfinished eleventh sample has advanced.
        BandwidthTracker tracker;
        for (int i = 1; i <= 10; ++i) add(tracker, 100000 - i * 250, 31250);
        for (int phase : {0, 1, 125, 249})
            QCOMPARE(tracker.getAverageMbpsAt(steady_clock::time_point(milliseconds(100000 + phase))), 1.0);
    }
    void unfinishedAndExpiredSamplesAreExcluded() {
        BandwidthTracker tracker;
        add(tracker, 100000, 1000000);
        QCOMPARE(tracker.getAverageMbpsAt(steady_clock::time_point(milliseconds(100125))), 0.0);
        add(tracker, 99750, 31250);
        QCOMPARE(tracker.getAverageMbpsAt(steady_clock::time_point(milliseconds(100000))), 1.0);
        QCOMPARE(tracker.getAverageMbpsAt(steady_clock::time_point(milliseconds(105000))), 0.0);
    }
    void quietIntervalsCountAsZeroThroughput() {
        BandwidthTracker tracker;
        add(tracker, 99000, 31250); // 0.25 Mbit during one second, then silence.
        QCOMPARE(tracker.getAverageMbpsAt(steady_clock::time_point(milliseconds(100000))), 0.25);
    }
};
QTEST_GUILESS_MAIN(BandwidthTest)
#include "bandwidth_test.moc"
