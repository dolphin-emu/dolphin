// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.activities

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.DocumentsContract
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.color.MaterialColors
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.ActivityUserDataBinding
import org.dolphinemu.dolphinemu.dialogs.NotificationDialog
import org.dolphinemu.dolphinemu.dialogs.TaskDialog
import org.dolphinemu.dolphinemu.dialogs.UserDataImportWarningDialog
import org.dolphinemu.dolphinemu.features.DocumentProvider
import org.dolphinemu.dolphinemu.model.TaskViewModel
import org.dolphinemu.dolphinemu.utils.*
import org.dolphinemu.dolphinemu.utils.ThemeHelper.enableScrollTint
import org.dolphinemu.dolphinemu.utils.ThemeHelper.setNavigationBarColor
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream
import java.util.zip.ZipOutputStream

class UserDataActivity : AppCompatActivity() {
    private lateinit var taskViewModel: TaskViewModel

    private lateinit var mBinding: ActivityUserDataBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        taskViewModel = ViewModelProvider(this)[TaskViewModel::class.java]

        ThemeHelper.setTheme(this)

        super.onCreate(savedInstanceState)

        mBinding = ActivityUserDataBinding.inflate(layoutInflater)
        setContentView(mBinding.root)

        WindowCompat.setDecorFitsSystemWindows(window, false)

        val android7 = Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
        val android10 = Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
        val android11 = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
        val legacy = DirectoryInitialization.isUsingLegacyUserDirectory()

        val userDataNewLocation =
            if (android10) R.string.user_data_new_location_android_10 else R.string.user_data_new_location
        mBinding.textType.setText(if (legacy) R.string.user_data_old_location else userDataNewLocation)

        val openFileManagerStringId =
            if (android7) R.string.user_data_open_user_folder else R.string.user_data_open_system_file_manager
        mBinding.buttonOpenSystemFileManager.setText(openFileManagerStringId)

        mBinding.textPath.text = DirectoryInitialization.getUserDirectory()

        mBinding.textAndroid11.visibility = if (android11 && !legacy) View.VISIBLE else View.GONE

        mBinding.buttonOpenSystemFileManager.visibility = if (android11) View.VISIBLE else View.GONE
        mBinding.buttonOpenSystemFileManager.setOnClickListener { openFileManager() }

        mBinding.buttonImportUserData.setOnClickListener { importUserData() }

        mBinding.buttonExportUserData.setOnClickListener { exportUserData() }

        setSupportActionBar(mBinding.toolbarUserData)
        supportActionBar!!.setDisplayHomeAsUpEnabled(true)

        setInsets()
        enableScrollTint(this, mBinding.toolbarUserData, mBinding.appbarUserData)
    }

    override fun onSupportNavigateUp(): Boolean {
        onBackPressed()
        return true
    }

    public override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)

        if (requestCode == REQUEST_CODE_IMPORT && resultCode == RESULT_OK) {
            val arguments = Bundle()
            arguments.putString(
                UserDataImportWarningDialog.KEY_URI_RESULT,
                data!!.data!!.toString()
            )

            val dialog = UserDataImportWarningDialog()
            dialog.arguments = arguments
            dialog.show(supportFragmentManager, UserDataImportWarningDialog.TAG)
        } else if (requestCode == REQUEST_CODE_EXPORT && resultCode == RESULT_OK) {
            taskViewModel.clear()
            taskViewModel.task = { exportUserData(data!!.data!!) }

            val arguments = Bundle()
            arguments.putInt(TaskDialog.KEY_TITLE, R.string.export_in_progress)
            arguments.putInt(TaskDialog.KEY_MESSAGE, 0)
            arguments.putBoolean(TaskDialog.KEY_CANCELLABLE, true)

            val dialog = TaskDialog()
            dialog.arguments = arguments
            dialog.show(supportFragmentManager, TaskDialog.TAG)
        }
    }

    private fun openFileManager() {
        // First, try to open the user data folder directly
        try {
            startActivity(getFileManagerIntentOnDocumentProvider(Intent.ACTION_VIEW))
            return
        } catch (_: ActivityNotFoundException) {}

        try {
            startActivity(getFileManagerIntentOnDocumentProvider("android.provider.action.BROWSE"))
            return
        } catch (_: ActivityNotFoundException) {}

        try {
            // Just try to open the file manager, try the package name used on "normal" phones
            startActivity(getFileManagerIntent("com.google.android.documentsui"))
            return
        } catch (_: ActivityNotFoundException) {}

        try {
            // Next, try the AOSP package name
            startActivity(getFileManagerIntent("com.android.documentsui"))
            return
        } catch (_: ActivityNotFoundException) {}

        try {
            // Activity not found. Perhaps it was removed by the OEM, or by some new Android version
            // that didn't exist at the time of writing. Not much we can do other than tell the user.
            val arguments = Bundle()
            arguments.putInt(
                NotificationDialog.KEY_MESSAGE,
                R.string.user_data_open_system_file_manager_failed
            )

            val dialog = NotificationDialog()
            dialog.arguments = arguments
            dialog.show(supportFragmentManager, NotificationDialog.TAG)
            return
        } catch (_: ActivityNotFoundException) {}
    }

    private fun getFileManagerIntent(packageName: String): Intent {
        // Fragile, but some phones don't expose the system file manager in any better way
        val intent = Intent(Intent.ACTION_MAIN)
        intent.setClassName(packageName, "com.android.documentsui.files.FilesActivity")
        intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
        return intent
    }

    private fun getFileManagerIntentOnDocumentProvider(action: String): Intent {
        val authority = "$packageName.user"
        val intent = Intent(action)
        intent.addCategory(Intent.CATEGORY_DEFAULT)
        intent.data = DocumentsContract.buildRootUri(authority, DocumentProvider.ROOT_ID)
        intent.addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION or Intent.FLAG_GRANT_PREFIX_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
        return intent
    }

    private fun importUserData() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT)
        intent.type = "application/zip"
        startActivityForResult(intent, REQUEST_CODE_IMPORT)
    }

    fun importUserData(source: Uri): Int {
        try {
            if (!isDolphinUserDataBackup(source))
                return R.string.user_data_import_invalid_file

            taskViewModel.mustRestartApp = true

            contentResolver.openInputStream(source).use { `is` ->
                ZipInputStream(`is`).use { zis ->
                    val userDirectory = File(DirectoryInitialization.getUserDirectory())
                    val userDirectoryCanonicalized = userDirectory.canonicalPath + '/'

                    deleteChildrenRecursively(userDirectory)

                    DirectoryInitialization.getGameListCache(this).delete()

                    var ze: ZipEntry? = zis.nextEntry
                    val buffer = ByteArray(BUFFER_SIZE)
                    while (ze != null) {
                        val destFile = File(userDirectory, ze.name)
                        val destDirectory = if (ze.isDirectory) destFile else destFile.parentFile

                        if (!destFile.canonicalPath.startsWith(userDirectoryCanonicalized)) {
                            Log.error("Zip file attempted path traversal! " + ze.name)
                            return R.string.user_data_import_failure
                        }

                        if (!destDirectory.isDirectory && !destDirectory.mkdirs()) {
                            throw IOException("Failed to create directory $destDirectory")
                        }

                        if (!ze.isDirectory) {
                            FileOutputStream(destFile).use { fos ->
                                var count: Int
                                while (zis.read(buffer).also { count = it } != -1) {
                                    fos.write(buffer, 0, count)
                                }
                            }

                            val time = ze.time
                            if (time > 0) {
                                destFile.setLastModified(time)
                            }
                        }
                        ze = zis.nextEntry
                    }
                }
            }
        } catch (e: IOException) {
            e.printStackTrace()
            return R.string.user_data_import_failure
        } catch (e: NullPointerException) {
            e.printStackTrace()
            return R.string.user_data_import_failure
        }
        return R.string.user_data_import_success
    }

    @Throws(IOException::class)
    private fun isDolphinUserDataBackup(uri: Uri): Boolean {
        contentResolver.openInputStream(uri).use { `is` ->
            ZipInputStream(`is`).use { zis ->
                var ze: ZipEntry
                while (zis.nextEntry.also { ze = it } != null) {
                    val name = ze.name
                    if (name == "Config/Dolphin.ini") {
                        return true
                    }
                }
            }
        }
        return false
    }

    @Throws(IOException::class)
    private fun deleteChildrenRecursively(directory: File) {
        val children =
            directory.listFiles() ?: throw IOException("Could not find directory $directory")
        for (child in children) {
            deleteRecursively(child)
        }
    }

    @Throws(IOException::class)
    private fun deleteRecursively(file: File) {
        if (file.isDirectory) {
            deleteChildrenRecursively(file)
        }

        if (!file.delete()) {
            throw IOException("Failed to delete $file")
        }
    }

    private fun exportUserData() {
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT)
        intent.type = "application/zip"
        intent.putExtra(Intent.EXTRA_TITLE, "dolphin-emu.zip")
        startActivityForResult(intent, REQUEST_CODE_EXPORT)
    }

    private fun exportUserData(destination: Uri): Int {
        try {
            contentResolver.openOutputStream(destination).use { os ->
                ZipOutputStream(os).use { zos ->
                    exportUserData(
                        zos,
                        File(DirectoryInitialization.getUserDirectory()),
                        null
                    )
                }
            }
        } catch (e: IOException) {
            e.printStackTrace()
            return R.string.user_data_export_failure
        }
        return R.string.user_data_export_success
    }

    @Throws(IOException::class)
    private fun exportUserData(zos: ZipOutputStream, input: File, pathRelativeToRoot: File?) {
        if (input.isDirectory) {
            val children = input.listFiles() ?: throw IOException("Could not find directory $input")

            // Check if the coroutine was cancelled
            if (!taskViewModel.cancelled) {
                for (child in children) {
                    exportUserData(zos, child, File(pathRelativeToRoot, child.name))
                }
            }
            if (children.isEmpty() && pathRelativeToRoot != null) {
                zos.putNextEntry(ZipEntry(pathRelativeToRoot.path + '/'))
            }
        } else {
            FileInputStream(input).use { fis ->
                val buffer = ByteArray(BUFFER_SIZE)
                val entry = ZipEntry(pathRelativeToRoot!!.path)
                entry.time = input.lastModified()
                zos.putNextEntry(entry)
                var count: Int
                while (fis.read(buffer, 0, buffer.size).also { count = it } != -1) {
                    zos.write(buffer, 0, count)
                }
            }
        }
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(mBinding.appbarUserData) { _: View?, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())

            InsetsHelper.insetAppBar(insets, mBinding.appbarUserData)

            mBinding.scrollViewUserData.setPadding(insets.left, 0, insets.right, insets.bottom)

            InsetsHelper.applyNavbarWorkaround(insets.bottom, mBinding.workaroundView)
            setNavigationBarColor(
                this,
                MaterialColors.getColor(mBinding.appbarUserData, R.attr.colorSurface)
            )
            windowInsets
        }
    }

    companion object {
        private const val REQUEST_CODE_IMPORT = 0
        private const val REQUEST_CODE_EXPORT = 1

        private const val BUFFER_SIZE = 64 * 1024

        @JvmStatic
        fun launch(context: Context) {
            val launcher = Intent(context, UserDataActivity::class.java)
            context.startActivity(launcher)
        }
    }
}
