#include "AppState.hpp"

AppState::AppState() {
    mediaOverlay.lyrics.parseLrc(mediaOverlay.lyricSource);
}
