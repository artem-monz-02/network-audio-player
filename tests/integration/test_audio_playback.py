import subprocess
from pathlib import Path

import pytest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PLAYER = PROJECT_ROOT / "build" / "network_audio_player"

WAV_FILE = PROJECT_ROOT / "test-tone.wav"
FLAC_FILE = PROJECT_ROOT / "test-tone.flac"


def run_player(audio_file, gain="1.0", drive="0.0", timeout=15):
    return subprocess.run(
        [
            str(PLAYER),
            str(audio_file),
            gain,
            drive,
        ],
        capture_output=True,
        text=True,
        timeout=timeout,
        cwd=PROJECT_ROOT,
    )


@pytest.fixture(scope="session", autouse=True)
def check_player_exists():
    if not PLAYER.is_file():
        pytest.fail(
            "Player executable not found. "
            "Run: cmake --build build"
        )


@pytest.mark.parametrize(
    "audio_file",
    [WAV_FILE, FLAC_FILE],
)
def test_valid_audio_playback(audio_file):
    if not audio_file.exists():
        pytest.skip(f"Test audio file not found: {audio_file}")

    result = run_player(audio_file)

    assert result.returncode == 0, (
        f"Playback failed:\n{result.stdout}\n{result.stderr}"
    )


def test_missing_audio_file():
    missing_file = PROJECT_ROOT / "missing-audio-file.wav"

    assert not missing_file.exists()

    result = run_player(missing_file)

    assert result.returncode != 0


@pytest.mark.parametrize(
    "gain",
    ["invalid", "-1.0", "nan", "inf"],
)
def test_invalid_gain(gain):
    result = run_player(WAV_FILE, gain=gain)

    assert result.returncode != 0


@pytest.mark.parametrize(
    "drive",
    ["invalid", "-1.0", "nan", "inf"],
)
def test_invalid_saturation(drive):
    result = run_player(WAV_FILE, drive=drive)

    assert result.returncode != 0
