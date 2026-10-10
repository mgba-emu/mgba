/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.dialogs

import android.content.Context
import androidx.annotation.StringRes
import androidx.appcompat.app.AlertDialog
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import java.util.ArrayDeque
import java.util.Queue

data class DialogData(
    val title: String,
    val message: String?,
    val onConfirm: () -> Unit,
    val onDismiss: (() -> Unit)?,
    val onNeutralClick: (() -> Unit)?,
    val confirmText: String,
    val dismissText: String,
    val neutralText: String
)

object AlertDialogQueue {

    private val dialogQueue: Queue<DialogData> = ArrayDeque()
    private var isShowing: Boolean = false
    private var currentDialog: AlertDialog? = null

    fun showDialog(
        context: Context,
        title: String,
        message: String? = null,
        onConfirm: () -> Unit = {},
        onDismiss: (() -> Unit)? = null,
        onNeutralClick: (() -> Unit)? = null,
        confirmText: String = "OK",
        dismissText: String = "Cancel",
        neutralText: String = ""
    ) {
        dialogQueue.add(
            DialogData(title, message, onConfirm, onDismiss, onNeutralClick, confirmText, dismissText, neutralText)
        )
        processQueue(context)
    }

    fun showDialog(
        context: Context,
        @StringRes titleRes: Int,
        @StringRes messageRes: Int? = null,
        onConfirm: () -> Unit = {},
        onDismiss: (() -> Unit)? = null,
        onNeutralClick: (() -> Unit)? = null,
        @StringRes confirmTextRes: Int? = null,
        @StringRes dismissTextRes: Int? = null,
        @StringRes neutralTextRes: Int? = null
    ) {
        showDialog(
            context = context,
            title = context.getString(titleRes),
            message = messageRes?.let { context.getString(it) },
            onConfirm = onConfirm,
            onDismiss = onDismiss,
            onNeutralClick = onNeutralClick,
            confirmText = confirmTextRes?.let { context.getString(it) } ?: "OK",
            dismissText = dismissTextRes?.let { context.getString(it) } ?: "Cancel",
            neutralText = neutralTextRes?.let { context.getString(it) } ?: "",
        )
    }

    private fun processQueue(context: Context) {
        if (isShowing || dialogQueue.isEmpty()) return

        val dialogData = dialogQueue.poll() ?: return
        isShowing = true

        val builder = MaterialAlertDialogBuilder(context)
            .setTitle(dialogData.title)
            .setPositiveButton(
                if (dialogData.confirmText == "OK") context.getString(android.R.string.ok) else dialogData.confirmText
            ) { dialog, _ ->
                dialogData.onConfirm()
                dialog.dismiss()
            }
            .setNegativeButton(
                if (dialogData.dismissText == "Cancel") context.getString(android.R.string.cancel) else dialogData.dismissText
            ) { dialog, _ ->
                dialogData.onDismiss?.invoke()
                dialog.dismiss()
            }
            .setOnDismissListener {
                isShowing = false
                currentDialog = null
                processQueue(context)
            }

        dialogData.message?.let {
            builder.setMessage(it)
        }

        dialogData.onNeutralClick?.let { onNeutralClick ->
            builder.setNeutralButton(
                dialogData.neutralText
            ) { dialog, _ ->
                onNeutralClick.invoke()
                dialog.dismiss()
            }
        }

        currentDialog = builder.show()
    }

    @Suppress("unused")
    fun clearQueue() {
        dialogQueue.clear()
        currentDialog?.dismiss()
        currentDialog = null
        isShowing = false
    }
}