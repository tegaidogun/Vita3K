package org.vita3k.emulator.ui.screens.settings

import android.app.Activity
import android.content.Context
import android.content.ContextWrapper
import android.content.pm.ActivityInfo
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import org.vita3k.emulator.overlay.InputOverlay
import org.vita3k.emulator.overlay.OverlayConfig
import org.vita3k.emulator.ui.components.OverlayEditorPalette

@Composable
internal fun OverlayLayoutEditorDialog(
    overlayConfig: OverlayConfig,
    onDismiss: () -> Unit,
    layoutProfileId: String = ""
) {
    BackHandler(onBack = onDismiss)

    val activity = LocalContext.current.findActivity()
    var overlayView by remember { mutableStateOf<InputOverlay?>(null) }
    OverlayEditorWindowEffect(activity = activity)

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black)
    ) {
        AndroidView(
            factory = { context ->
                InputOverlay(context).apply {
                    overlayView = this
                    setLayoutProfileId(layoutProfileId)
                    setAllowVirtualController(false)
                    setAutoHideEnabled(false)
                    setIsInEditMode(true)
                    setScale(overlayConfig.overlayScale / 100f)
                    setOpacity(overlayConfig.overlayOpacity)
                    setState(overlayConfig.activeMask())
                }
            },
            update = { view ->
                overlayView = view
                view.setLayoutProfileId(layoutProfileId)
                view.setAllowVirtualController(false)
                view.setAutoHideEnabled(false)
                view.setIsInEditMode(true)
                view.setScale(overlayConfig.overlayScale / 100f)
                view.setOpacity(overlayConfig.overlayOpacity)
                view.setState(overlayConfig.activeMask())
            },
            modifier = Modifier.fillMaxSize()
        )

        OverlayEditorPalette(
            onDone = onDismiss,
            onReset = { overlayView?.resetButtonPlacement() }
        )
    }
}

@Composable
private fun OverlayEditorWindowEffect(activity: Activity?) {
    DisposableEffect(activity) {
        if (activity == null) {
            return@DisposableEffect onDispose { }
        }

        val previousOrientation = activity.requestedOrientation
        val window = activity.window
        val controller = WindowInsetsControllerCompat(window, window.decorView)

        activity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        WindowCompat.setDecorFitsSystemWindows(window, false)
        controller.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        controller.hide(WindowInsetsCompat.Type.systemBars())

        onDispose {
            activity.requestedOrientation = previousOrientation
            WindowCompat.setDecorFitsSystemWindows(window, true)
            controller.show(WindowInsetsCompat.Type.systemBars())
        }
    }
}

private tailrec fun Context.findActivity(): Activity? = when (this) {
    is Activity -> this
    is ContextWrapper -> baseContext.findActivity()
    else -> null
}
