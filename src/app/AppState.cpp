#include "AppState.hpp"

AppState::AppState() {
    config.mediaOverlay.lyrics.parseLrc(config.mediaOverlay.lyricSource);
}
