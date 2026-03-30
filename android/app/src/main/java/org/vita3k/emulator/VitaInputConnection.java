package org.vita3k.emulator;

import android.content.Context;
import android.text.Editable;
import android.view.KeyEvent;
import android.view.View;

import org.libsdl.app.SDLInputConnection;

public class VitaInputConnection extends SDLInputConnection {
    public VitaInputConnection(View targetView, boolean fullEditor) {
        super(targetView, fullEditor);
    }

    private boolean isNativeImeActive() {
        try {
            return NativeLib.INSTANCE.isImeActive();
        } catch (Throwable ignored) {
            return false;
        }
    }

    private Emulator getEmulator() {
        Context context = mEditText.getContext();
        return context instanceof Emulator ? (Emulator) context : null;
    }

    private void clearEditableState() {
        Editable content = getEditable();
        if (content != null) {
            content.clear();
            removeComposingSpans(content);
        }
        mCommittedText = "";
    }

    private void submitFromKeyboard() {
        Emulator emulator = getEmulator();
        if (emulator != null) {
            emulator.completeImeFromKeyboard(mEditText);
        } else {
            try {
                NativeLib.INSTANCE.submitIme();
            } catch (Throwable ignored) {
            }
            clearEditableState();
        }
    }

    private void commitToNativeIme(CharSequence text) {
        final String raw = text != null ? text.toString() : "";
        final boolean shouldSubmit = raw.indexOf('\n') >= 0 || raw.indexOf('\r') >= 0;
        final String filtered = raw.replace("\n", "").replace("\r", "");

        if (!filtered.isEmpty()) {
            try {
                NativeLib.INSTANCE.commitImeText(filtered);
            } catch (Throwable ignored) {
            }
        }

        clearEditableState();

        if (shouldSubmit) {
            submitFromKeyboard();
        }
    }

    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
        if (!super.commitText(text, newCursorPosition)) {
            return false;
        }

        if (!isNativeImeActive()) {
            return true;
        }

        commitToNativeIme(text);
        return true;
    }

    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
        if (!super.setComposingText(text, newCursorPosition)) {
            return false;
        }

        if (!isNativeImeActive()) {
            return true;
        }

        try {
            NativeLib.INSTANCE.setImeComposingText(text != null ? text.toString() : "");
        } catch (Throwable ignored) {
        }

        if (text == null || text.length() == 0) {
            clearEditableState();
        }

        return true;
    }

    @Override
    public boolean deleteSurroundingText(int beforeLength, int afterLength) {
        if (!isNativeImeActive()) {
            return super.deleteSurroundingText(beforeLength, afterLength);
        }

        if (android.os.Build.VERSION.SDK_INT <= 29 && beforeLength > 0 && afterLength == 0) {
            try {
                NativeLib.INSTANCE.backspaceIme(Math.max(beforeLength, 1));
            } catch (Throwable ignored) {
            }
            return true;
        }

        if (!super.deleteSurroundingText(beforeLength, afterLength)) {
            return false;
        }

        if (beforeLength > 0) {
            try {
                NativeLib.INSTANCE.backspaceIme(beforeLength);
            } catch (Throwable ignored) {
            }
        }

        Editable content = getEditable();
        if (content == null || content.length() == 0) {
            try {
                NativeLib.INSTANCE.setImeComposingText("");
            } catch (Throwable ignored) {
            }
            mCommittedText = "";
        }

        return true;
    }

    @Override
    public boolean finishComposingText() {
        if (!super.finishComposingText()) {
            return false;
        }

        if (isNativeImeActive()) {
            try {
                NativeLib.INSTANCE.setImeComposingText("");
            } catch (Throwable ignored) {
            }
            clearEditableState();
        }

        return true;
    }

    @Override
    public boolean performEditorAction(int actionCode) {
        if (isNativeImeActive()) {
            submitFromKeyboard();
            return true;
        }

        return super.performEditorAction(actionCode);
    }

    @Override
    public boolean sendKeyEvent(KeyEvent event) {
        if (isNativeImeActive()
                && event.getAction() == KeyEvent.ACTION_UP
                && (event.getKeyCode() == KeyEvent.KEYCODE_ENTER
                || event.getKeyCode() == KeyEvent.KEYCODE_NUMPAD_ENTER)) {
            submitFromKeyboard();
            return true;
        }

        return super.sendKeyEvent(event);
    }
}
