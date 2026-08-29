import QtQuick 2.0
import Sailfish.Silica 1.0

// Drop-in SilicaListView replacement that adds a clean bottom-edge overscroll bounce (pull,
// darken, spring back) for the one case Silica itself can't do this for on its own: a list
// short enough that its content doesn't actually overflow the viewport (an empty list, or one
// with only a handful of items). A no-op the instant a list's content genuinely exceeds the
// viewport - Silica's own native drag-past-bounds/dim already works there and keeps doing so
// completely unchanged (this component doesn't touch boundsBehavior or contentY at all).
//
// Why this exists instead of a boundsBehavior/PushUpMenu tweak (all tried first, all rejected -
// see harbour-sailreader's git history for the full live-debugged derivation):
//
// - SilicaListView's own boundsBehavior binding only allows real drag-past-bounds (and the
//   elastic dim that comes with it, baked into every SilicaListView via its private
//   BoundsBehavior/BounceEffect) at an edge while that edge's own Pulley menu is at its rest
//   position (PulleyMenuBase._activationPermitted/_atInitialPosition) - a bottom edge with no
//   PushUpMenu never satisfies that once scrolled away from the top, hard-stopping instead.
// - PullDownMenu.qml has its own additional, explicit safeguard for content shorter than the
//   viewport specifically (_maxDragPosition - "if there is no push up menu ... we must also
//   prevent moving in the wrong direction"), which blocks the bottom edge regardless of any
//   boundsBehavior/bottomMargin override applied from outside.
// - Adding an actual (even empty) PushUpMenu unlocks that safeguard, but PushUpMenu's own
//   HighlightBar - a private, non-overridable child of PulleyMenuBase, colored from the
//   ambience palette rather than a per-instance property - then reads as a real "there's a
//   menu here" affordance where there is none.
//
// Instead, this reads the transient contentY values Flickable computes each frame while STILL
// discarding them (a blocked drag still produces a real, monotonically growing sequence each
// time - e.g. -183, -133, -83, ... - immediately followed by a reset back to originY) and
// drives its own separate, purely visual Translate + opacity dim from that signal - contentY
// itself is left alone throughout, so none of Silica's own menu/highlight machinery is ever
// involved. The dimming curve is ported verbatim from Sailfish Silica's own private/
// BoundsBehavior.qml (BSD-licensed, plain QML), so a short list's bottom bounce reads as the
// same interaction language as a real Silica pull elsewhere in the app, not a bespoke one-off.
SilicaListView {
    id: bounceListView

    transform: Translate {
        id: _bottomBounceTranslate
        y: 0
    }

    // Running max of (contentY - originY) observed so far in the current drag - using the max
    // rather than the raw (alternating growing-value/reset) signal directly is what turns that
    // noisy sequence into the smooth, monotonically growing pull distance a real elastic drag
    // would produce. Reset to 0 at the start of every new drag.
    property real _bottomPullDistance: 0

    // Square-root damping - a diminishing-returns resistance curve, same shape a real elastic
    // pull uses (the further you've already pulled, the less an additional pixel of finger
    // movement visibly adds), capped well short of a full row height so it reads as "give",
    // never as an actual scroll.
    function _bottomBounceOffset(pullDistance) {
        return Math.min(Math.sqrt(Math.max(pullDistance, 0)) * 2.2, Theme.itemSizeMedium)
    }

    // Same formula/constants as Sailfish Silica's own private/BoundsBehavior.qml - reusing it
    // rather than inventing a bespoke curve is what makes this read as a real Silica pull.
    // Theme.opacityLow is the floor (never fades fully invisible), Theme.itemSizeExtraLarge * 2
    // is the pull distance at which that floor is reached.
    function _bottomBounceOpacity(pullDistance) {
        return Theme.opacityLow + (1.0 - Theme.opacityLow) * Math.pow(1.0
            - Math.min(pullDistance, Theme.itemSizeExtraLarge * 2) / (Theme.itemSizeExtraLarge * 2), 1.5)
    }

    // Connections rather than plain onDraggingChanged/onContentYChanged directly on this root
    // item - a consumer instantiating BounceListView { onDraggingChanged: ... } for its own,
    // unrelated purpose would otherwise silently replace this component's own handler instead
    // of layering alongside it (a single onX: slot per object, not additive) - Connections
    // keeps this internal regardless of what a consumer also hooks on the instance itself.
    Connections {
        target: bounceListView
        onDraggingChanged: {
            if (bounceListView.dragging) {
                bounceListView._bottomPullDistance = 0
                // A new drag starting before the previous one's spring-back finished would
                // otherwise fight it - the animation "holds" translate.y/opacity while
                // running, silently overriding onContentYChanged's own imperative writes.
                _bottomBounceSpringBack.stop()
            } else {
                _bottomBounceSpringBack.restart()
            }
        }
        onContentYChanged: {
            if (!bounceListView.dragging || bounceListView.contentHeight > bounceListView.height) return
            var rawPull = bounceListView.contentY - bounceListView.originY
            if (rawPull > bounceListView._bottomPullDistance) {
                bounceListView._bottomPullDistance = rawPull
                _bottomBounceTranslate.y = -bounceListView._bottomBounceOffset(rawPull)
                bounceListView.opacity = bounceListView._bottomBounceOpacity(rawPull)
            }
        }
    }

    // Springs the Translate + opacity back to rest once the drag that produced them ends.
    ParallelAnimation {
        id: _bottomBounceSpringBack
        // OutBack overshoots slightly past 0 before settling, reading as a genuine spring
        // rather than a linear slide back.
        NumberAnimation {
            target: _bottomBounceTranslate
            property: "y"
            to: 0
            duration: 300
            easing.type: Easing.OutBack
        }
        // Same duration/easing as Sailfish Silica's own private/FadeAnimation.qml, which is
        // exactly what fades the native dim back to full opacity elsewhere in this app (e.g.
        // the top PullDownMenu pull) - matching it here keeps the two feeling consistent.
        NumberAnimation {
            target: bounceListView
            property: "opacity"
            to: 1.0
            duration: 200
            easing.type: Easing.InOutQuad
        }
    }
}
