package com.example.videoapp.ui.dialog

import android.app.AlertDialog
import android.app.Dialog
import android.os.Bundle
import androidx.fragment.app.DialogFragment

class ErrorDialog : DialogFragment() {
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return AlertDialog.Builder(activity)
            .setMessage(arguments?.getString(ARG_MESSAGE))
            .setPositiveButton(
                android.R.string.ok
            ) { _, _ ->
                activity?.finish()
            }
            .create()
    }

    companion object {
        private val ARG_MESSAGE = "message"

        fun newInstance(message: String): ErrorDialog {
            val dialog = ErrorDialog()
            val args = Bundle()
            args.putString(ARG_MESSAGE, message)
            dialog.arguments = args
            return dialog
        }
    }

}