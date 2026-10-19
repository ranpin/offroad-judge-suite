#!/usr/bin/env bash
# One-command, self-contained end-to-end demo — NO external exam data required.
#
# Builds the repo's own mock-client/od from source (out-of-tree), stages a
# throwaway run dir from the tiny synthetic sample-data/, then drives the built
# competitor binary through the full wire protocol via judge_server.py:
#     START(02 30 03) -> BEGIN(02 31 03) -> token frames -> answer frames
# and asserts at least one decoded answer frame came back (the closed loop).
#
# Requires: bash, qmake(Qt5)+g+++libopencv (to build C), python3 (stdlib only).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"   # tools/soft-judge-reference
REPO="$(cd "$HERE/../.." && pwd)"
SAMPLE="$HERE/sample-data"
BUILD="$(mktemp -d)"
RUN="$(mktemp -d)"

cleanup() {
    pkill -f "$BUILD/MockClient" 2>/dev/null || true
    rm -rf "$BUILD" "$RUN"
}
trap cleanup EXIT

echo "[demo] 1/4 building mock-client/od from source (out-of-tree) ..."
cp "$REPO"/mock-client/od/*.cpp "$REPO"/mock-client/od/*.h \
   "$REPO"/mock-client/od/*.hpp "$REPO"/mock-client/od/MockClient.pro "$BUILD"/
( cd "$BUILD" && qmake MockClient.pro && make -j"$(nproc)" ) >/tmp/mockclient_build.log 2>&1 \
    || { echo "[demo] BUILD FAILED"; tail -30 /tmp/mockclient_build.log; exit 1; }
echo "[demo] built: $BUILD/MockClient"

echo "[demo] 2/4 staging run dir from synthetic sample-data ..."
cat > "$RUN/config.yml" <<EOF
%YAML:1.0
picture_path: "$SAMPLE/"
timeStamp_path: "$SAMPLE/camera-timestamp.csv"
EOF
mkdir -p "$RUN/data/exams/EXAM-OD-001/Raw-001-Groundtruth"
cp "$SAMPLE/sample-gt.csv" "$RUN/data/exams/EXAM-OD-001/Raw-001-Groundtruth/gt_detection.csv"

echo "[demo] 3/4 launching competitor + driving closed loop ..."
( cd "$RUN" && printf 'n\n0\n' | "$BUILD/MockClient" > "$RUN/C.log" 2>&1 ) &
for _ in $(seq 1 40); do
    grep -qa 'Waiting for start signal' "$RUN/C.log" && break
    sleep 0.5
done
if ! grep -qa 'Waiting for start signal' "$RUN/C.log"; then
    echo "[demo] FAIL: competitor did not reach 'Waiting for start signal'"; sed -n '1,20p' "$RUN/C.log"; exit 1
fi

python3 "$HERE/judge_server.py" --token-csv "$SAMPLE/sample-tokens.csv" --target-answers 3 \
    > "$RUN/J.log" 2>&1 || true
cat "$RUN/J.log"

echo "[demo] 4/4 verdict ..."
if grep -q 'Receive start signal.' "$RUN/C.log" \
   && grep -Eq '答案帧\(7003\): [1-9]' "$RUN/J.log"; then
    echo "[demo] PASS: repo-only end-to-end closed loop reproduced (START->token->answer)."
    exit 0
else
    echo "[demo] FAIL: closed loop not observed. C.log:"; sed -n '1,20p' "$RUN/C.log"; exit 1
fi
