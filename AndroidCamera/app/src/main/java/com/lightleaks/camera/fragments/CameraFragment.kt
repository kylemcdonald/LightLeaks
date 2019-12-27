/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.lightleaks.camera.fragments

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Color
import android.graphics.ImageFormat
import android.graphics.drawable.ColorDrawable
import android.hardware.camera2.*
import android.hardware.camera2.params.MeteringRectangle
import android.media.Image
import android.media.ImageReader
import android.media.RingtoneManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.HandlerThread
import android.util.Log
import android.view.*
import android.widget.Toast
import androidx.fragment.app.Fragment
import androidx.lifecycle.Observer
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavController
import androidx.navigation.Navigation
import androidx.navigation.fragment.navArgs
import com.lightleaks.android.camera2.common.computeExifOrientation
import com.lightleaks.android.camera2.common.getPreviewOutputSize
import com.lightleaks.android.camera2.common.AutoFitSurfaceView
import com.lightleaks.android.camera2.common.OrientationLiveData
import com.lightleaks.camera.CameraActivity.Companion.ANIMATION_FAST_MILLIS
import com.lightleaks.camera.CameraActivity.Companion.ANIMATION_SLOW_MILLIS
import com.lightleaks.camera.R
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import okhttp3.*
import okio.ByteString
import okio.ByteString.Companion.toByteString
import java.io.*
import java.text.SimpleDateFormat
import java.util.concurrent.ArrayBlockingQueue
import java.util.concurrent.TimeoutException
import java.util.Date
import java.util.Locale
import kotlin.RuntimeException
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException
import kotlin.coroutines.suspendCoroutine
import kotlin.math.max


class CameraFragment : Fragment() {

    /** AndroidX navigation arguments */
    private val args: CameraFragmentArgs by navArgs()

    /** Host's navigation controller */
    private val navController: NavController by lazy {
        Navigation.findNavController(requireActivity(), R.id.fragment_container)
    }

    /** Detects, characterizes, and connects to a CameraDevice (used for all camera operations) */
    private val cameraManager: CameraManager by lazy {
        val context = requireContext().applicationContext
        context.getSystemService(Context.CAMERA_SERVICE) as CameraManager
    }

    /** [CameraCharacteristics] corresponding to the provided Camera ID */
    private val characteristics: CameraCharacteristics by lazy {
        cameraManager.getCameraCharacteristics(args.cameraId)
    }

    /** Readers used as buffers for camera still shots */
    private lateinit var imageReader: ImageReader

    /** [HandlerThread] where all camera operations run */
    private val cameraThread = HandlerThread("CameraThread").apply { start() }

    /** [Handler] corresponding to [cameraThread] */
    private val cameraHandler = Handler(cameraThread.looper)

    /** [HandlerThread] where all buffer reading operations run */
    private val imageReaderThread = HandlerThread("imageReaderThread").apply { start() }

    /** [Handler] corresponding to [imageReaderThread] */
    private val imageReaderHandler = Handler(imageReaderThread.looper)

    /** Where the camera preview is displayed */
    private lateinit var viewFinder: SurfaceView

    /** Internal reference to the ongoing [CameraCaptureSession] configured with our parameters */
    private lateinit var session: CameraCaptureSession

    /** Live data listener for changes in the device orientation relative to the camera */
    private lateinit var relativeOrientation: OrientationLiveData

//    private var socket: Socket = IO.socket("http://192.168.86.164:9000/cam");
    private var client: OkHttpClient = OkHttpClient()

    private lateinit var captureRequest: CaptureRequest.Builder;
    private var exposureTime: Double = 0.01;
    private var iso: Int = 55;
    private var opticalStabilization: Boolean = false;
    private var noiseReduction: Boolean = false;
    private var torch: Boolean = false;

    private lateinit var websocketClient: WebSocket

    private var manualFocusEngaged = false;

    override fun onCreateView(
            inflater: LayoutInflater,
            container: ViewGroup?,
            savedInstanceState: Bundle?
    ): View? = AutoFitSurfaceView(requireContext())

    @SuppressLint("MissingPermission")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        viewFinder = view as AutoFitSurfaceView

        view.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceDestroyed(holder: SurfaceHolder) = Unit

            override fun surfaceChanged(
                    holder: SurfaceHolder,
                    format: Int,
                    width: Int,
                    height: Int) = Unit

            override fun surfaceCreated(holder: SurfaceHolder) {

                // Selects appropriate preview size and configures view finder
                val previewSize = getPreviewOutputSize(
                        viewFinder.display, characteristics, SurfaceHolder::class.java)
                Log.d(TAG, "View finder size: ${viewFinder.width} x ${viewFinder.height}")
                Log.d(TAG, "Selected preview size: $previewSize")
                view.holder.setFixedSize(previewSize.width, previewSize.height)
                view.setAspectRatio(previewSize.width, previewSize.height)

                // To ensure that size is set, initialize camera in the view's thread
                view.post {
                    initializeCamera()
                    initializeWs()
                }

            }
        })

        // Used to rotate the output video to match device orientation
        relativeOrientation = OrientationLiveData(requireContext(), characteristics).apply {
            observe(this@CameraFragment, Observer {
                orientation -> Log.d(TAG, "Orientation changed: $orientation")
            })
        }
    }

    private fun setCameraSettings(captureRequest: CaptureRequest.Builder) {
        val isoRange = characteristics.get(CameraCharacteristics.SENSOR_INFO_SENSITIVITY_RANGE);
        Log.i("CAM", "ISO range: "+isoRange);

        val exposureTimeRange = characteristics.get(CameraCharacteristics.SENSOR_INFO_EXPOSURE_TIME_RANGE);
        Log.i("CAM", "exposureTimeRange: "+exposureTimeRange);

        val frameDurationRange = characteristics.get(CameraCharacteristics.SENSOR_INFO_MAX_FRAME_DURATION);
        Log.i("CAM", "frameDurationRange: "+frameDurationRange);

        val tonemap = characteristics.get(CameraCharacteristics.TONEMAP_AVAILABLE_TONE_MAP_MODES);
        Log.i("CAM", "Tonemap curves" + tonemap)


        Log.i("CAM", "Exposure Time " + exposureTime)
        Log.i("CAM", "ISO " + iso)

//        captureRequest.set(CaptureRequest.CONTROL_MODE, CaptureRequest.CONTROL_MODE_OFF)

        captureRequest.set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_OFF);
        captureRequest.set(CaptureRequest.SENSOR_EXPOSURE_TIME, (exposureTime * 1e+9).toLong());
        captureRequest.set(CaptureRequest.SENSOR_SENSITIVITY, iso);
        captureRequest.set(CaptureRequest.SENSOR_FRAME_DURATION, 10);
        captureRequest.set(CaptureRequest.DISTORTION_CORRECTION_MODE, CaptureRequest.DISTORTION_CORRECTION_MODE_HIGH_QUALITY);
        captureRequest.set(CaptureRequest.EDGE_MODE, CaptureRequest.EDGE_MODE_OFF);
        captureRequest.set(CaptureRequest.JPEG_QUALITY, 100);
        captureRequest.set(CaptureRequest.LENS_OPTICAL_STABILIZATION_MODE, if(opticalStabilization) CaptureRequest.LENS_OPTICAL_STABILIZATION_MODE_ON else CaptureRequest.LENS_OPTICAL_STABILIZATION_MODE_OFF)
        captureRequest.set(CaptureRequest.CONTROL_AWB_MODE, CaptureRequest.CONTROL_AWB_MODE_FLUORESCENT)
        captureRequest.set(CaptureRequest.CONTROL_VIDEO_STABILIZATION_MODE, CaptureRequest.CONTROL_VIDEO_STABILIZATION_MODE_OFF)

        captureRequest.set(CaptureRequest.NOISE_REDUCTION_MODE, if(noiseReduction) CaptureRequest.NOISE_REDUCTION_MODE_HIGH_QUALITY else CaptureRequest.NOISE_REDUCTION_MODE_OFF)

        captureRequest.set(CaptureRequest.FLASH_MODE, if(torch) CaptureRequest.FLASH_MODE_TORCH else CaptureRequest.FLASH_MODE_OFF)

        captureRequest.set(CaptureRequest.STATISTICS_OIS_DATA_MODE, CaptureRequest.STATISTICS_OIS_DATA_MODE_ON)




    }

    /**
     * Begin all camera operations in a coroutine in the main thread. This function:
     * - Opens the camera
     * - Configures the camera session
     * - Starts the preview by dispatching a repeating capture request
     * - Sets up the image capture flisteners
     */
    private fun initializeCamera() = lifecycleScope.launch(Dispatchers.Main) {
        val camera = openCamera()
        session = startCaptureSession(camera)
        captureRequest = camera.createCaptureRequest(
                CameraDevice.TEMPLATE_PREVIEW).apply { addTarget(viewFinder.holder.surface) }

        setCameraSettings(captureRequest)
        // This will keep sending the capture request as frequently as possible until the
        // session is torn down or session.stopRepeating() is called
        session.setRepeatingRequest(captureRequest.build(), null, cameraHandler)

        view?.setOnTouchListener(View.OnTouchListener { v, event ->
            if (event.actionMasked != MotionEvent.ACTION_DOWN) {
                return@OnTouchListener false;
            }
            if (manualFocusEngaged) {
                Log.d(TAG, "Manual focus already engaged");
                return@OnTouchListener true;
            }

            val sensorArraySize = characteristics.get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE);
            val y = (( 1 - event.x / v.width.toDouble())  * sensorArraySize!!.height()).toInt();
            val x = ((event.y / v.height.toDouble()) * sensorArraySize.width()).toInt();

            focus(x,y) { complete ->
                Log.i(TAG, "Focus complete: " + complete)
            }
            return@OnTouchListener true
        })
    }

    private fun focus(x: Int, y: Int, onFocusComplete : (success: Boolean) -> Unit ) {
        if (manualFocusEngaged) {
            Log.d(TAG, "Manual focus already engaged");
        }

        val halfTouchWidth  = 150
        val halfTouchHeight = 150
        val focusAreaTouch = MeteringRectangle(max(x - halfTouchWidth,  0),
                max(y - halfTouchHeight, 0),
                halfTouchWidth  * 2,
                halfTouchHeight * 2,
                MeteringRectangle.METERING_WEIGHT_MAX - 1);


        val captureCallbackHandler = object: CameraCaptureSession.CaptureCallback() {
            override fun onCaptureCompleted(session: CameraCaptureSession, request: CaptureRequest, result: TotalCaptureResult) {
                super.onCaptureCompleted(session, request, result)
                manualFocusEngaged = false;
                if (request.getTag() == "FOCUS_TAG") {
                    //the focus trigger is complete -
                    //resume repeating (preview surface will get frames), clear AF trigger
                    captureRequest.setTag((""))
                    captureRequest.set(CaptureRequest.CONTROL_AF_TRIGGER, null);
                    session.setRepeatingRequest(captureRequest.build(), null, null);
                    onFocusComplete(true)
                }
            }

            override fun onCaptureFailed(session: CameraCaptureSession, request: CaptureRequest, failure: CaptureFailure) {
                super.onCaptureFailed(session, request, failure)
                Log.e(TAG, "Manual AF failure: " + failure);
                manualFocusEngaged = false;
                onFocusComplete(false)
            }
        }

        //first stop the existing repeating request
        session.stopRepeating();

        //cancel any existing AF trigger (repeated touches, etc.)
        captureRequest.set(CaptureRequest.CONTROL_AF_TRIGGER, CameraMetadata.CONTROL_AF_TRIGGER_CANCEL);
        captureRequest.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_OFF);
        session.capture(captureRequest.build(), captureCallbackHandler, cameraHandler);

        //Now add a new AF trigger with focus region
        captureRequest.set(CaptureRequest.CONTROL_AF_REGIONS, arrayOf(focusAreaTouch));

        captureRequest.set(CaptureRequest.CONTROL_MODE, CameraMetadata.CONTROL_MODE_AUTO)   ;
        captureRequest.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_AUTO);
        captureRequest.set(CaptureRequest.CONTROL_AF_TRIGGER, CameraMetadata.CONTROL_AF_TRIGGER_START);
        captureRequest.setTag("FOCUS_TAG"); //we'll capture this later for resuming the preview

        //then we ask for a single request (not repeating!)
        session.capture(captureRequest.build(), captureCallbackHandler, cameraHandler);
        manualFocusEngaged = true;
    }

//    private fun isMeteringAreaAFSupported(): Boolean {
//        return characteristics.get(CameraCharacteristics.CONTROL_MAX_REGIONS_AF) >= 1;
//    }

    private var lastOffsetX = 0.0;
    private var lastOffsetY = 0.0;

    private fun sendPhoto() {
        lifecycleScope.launch(Dispatchers.IO) {
            takePhoto().use { result ->
//                val focusDistance = result.metadata.get(CaptureResult.LENS_FOCUS_DISTANCE)
//                Log.i(TAG, "Focus distance "+   focusDistance!!)

//                val ois = result.metadata.get(CaptureResult.STATISTICS_OIS_SAMPLES)
//
//                var avgX = 0.0;
//                var avgY = 0.0;
//                for(frame in ois!!.iterator()) {
////                    Log.i(TAG, ""+ frame.timestamp)
//                    avgX += frame.xshift;
//                    avgY += frame.yshift
//                }
//                avgX /= ois.size
//                avgY /= ois.size


//                avgX = ois[0].xshift.toDouble()
//                avgY = ois[0].yshift.toDouble()
//                Log.i(TAG, "X: " + avgX + "Y: "+avgY)
//                Log.i(TAG, "First X: " + ois[0].xshift  + "Y: "+ois[0].yshift )
//                Log.i(TAG, /"X: " + (lastOffsetX - avgX)+ "Y: "+(lastOffsetY - avgY))

//                lastOffsetX = avgX
//                lastOffsetY = avgY

                val buffer = result.image.planes[0].buffer
                val bytes = ByteArray(buffer.remaining()).apply { buffer.get(this) }
                websocketClient.send(bytes.toByteString())

//                websocketClient.send("oisOffset:"+avgX+":"+avgY)
//                websocketClient.send("next")
            }
        }
    }

    private fun initializeWs() = lifecycleScope.launch(Dispatchers.Main) {
        Log.i(TAG, args.ip)

        val request = Request.Builder().url("ws://"+args.ip+":9000/cam").build();
//                val listener = new EchoWebSocketListener();
        websocketClient = client.newWebSocket(request, object: WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                super.onOpen(webSocket, response)
                Log.i("SOCKET", "onOpen")

                requireActivity().runOnUiThread() {
                    Toast.makeText(requireContext(),  "Connected!", Toast.LENGTH_SHORT).show()
                }

                webSocket.send("Hello")
            }

            override fun onMessage(webSocket: WebSocket, bytes: ByteString) {
                Log.i("SOCKET", "onByteMessage")
                super.onMessage(webSocket, bytes)
            }

            override fun onMessage(webSocket: WebSocket, text: String) {
                Log.i("SOCKET", "onTxtMessage " + text)
                if(text == "preview") {
                    sendPhoto()
                } else if(text.startsWith("photo:")) {
                    val ar = text.split(':');
                    sendPhoto()
                } else if (text.equals("scanComplete")) {
                    val notification = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION);
                    val r = RingtoneManager.getRingtone(requireContext().applicationContext, notification);
                    r.play();

                } else if(text.startsWith("setConfig:")) {
                    val ar = text.split(':');
                    Log.i(TAG, "setConfig: "+ar)

//                    if( captureRequest == null) return

                    if(ar[1] == "focus") {
                        focus(ar[2].toInt() , ar[3].toInt()) { complete ->
                            webSocket.send("setConfigComplete:" + ar[1])
                        }
                    } else {
                        if (ar[1] == "exposureTime") {
                            exposureTime = ar[2].toDouble()
                        } else if (ar[1] == "iso") {
                            iso = ar[2].toInt()
                        } else if(ar[1] == "opticalStabilization") {
                            opticalStabilization = ar[2].toBoolean()
                        } else if(ar[1] == "noiseReduction") {
                            noiseReduction = ar[2].toBoolean()
                        } else if(ar[1] == "torch") {
                            torch  = ar[2].toBoolean()
                        }

                        setCameraSettings(captureRequest)
                        session.setRepeatingRequest(captureRequest.build(), null, cameraHandler)

                        webSocket.send("setConfigComplete:" + ar[1])
                    }
                }
                super.onMessage(webSocket, text)
            }

            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                super.onFailure(webSocket, t, response)

                requireActivity().runOnUiThread() {
                    Toast.makeText(requireContext(),  "Could not connect to websocket", Toast.LENGTH_LONG).show()

                }
                Log.i("SOCKET", "onFailure" + t)

            }

            override fun onClosed(webSocket: WebSocket, code: Int, reason: String) {
                super.onClosed(webSocket, code, reason)

                requireActivity().runOnUiThread() {
                    Toast.makeText(requireContext(),  "Disconnected from websocket", Toast.LENGTH_LONG).show()

                }
                Log.i("SOCKET", "onClosed")

            }
        })
    }
    /** Opens the camera and returns the opened device (as the result of the suspend coroutine) */
    @SuppressLint("MissingPermission")
    private suspend fun openCamera(): CameraDevice = suspendCoroutine { cont ->
        val cameraManager =
                requireContext().getSystemService(Context.CAMERA_SERVICE) as CameraManager

        val cameraId = args.cameraId
        cameraManager.openCamera(cameraId, object : CameraDevice.StateCallback() {
            override fun onDisconnected(device: CameraDevice) {
                Log.w(TAG, "Camera $cameraId has been disconnected")
                requireActivity().finish()
            }

            override fun onError(device: CameraDevice, error: Int) {
                val exc = RuntimeException("Camera $cameraId open error: $error")
                Log.e(TAG, exc.message, exc)
                cont.resumeWithException(exc)
            }

            override fun onOpened(device: CameraDevice) = cont.resume(device)

        }, cameraHandler)
    }

    /**
     * Starts a [CameraCaptureSession] and returns the configured session (as the result of the
     * suspend coroutine
     */
    private suspend fun startCaptureSession(device: CameraDevice):
            CameraCaptureSession = suspendCoroutine { cont ->
        val size = characteristics.get(
                CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)!!
                .getOutputSizes(args.pixelFormat).maxBy { it.height * it.width }!!

        imageReader =
            ImageReader.newInstance(size.width, size.height, args.pixelFormat, IMAGE_BUFFER_SIZE)
        // Create list of Surfaces where the camera will output frames
        val targets: MutableList<Surface> =
                arrayOf(viewFinder.holder.surface, imageReader.surface).toMutableList()

        // Create a capture session using the predefined targets; this also involves defining the
        // session state callback to be notified of when the session is ready
        device.createCaptureSession(targets, object: CameraCaptureSession.StateCallback() {

            override fun onConfigureFailed(session: CameraCaptureSession) {
                val exc = RuntimeException(
                        "Camera ${device.id} session configuration failed, see log for details")
                Log.e(TAG, exc.message, exc)
                cont.resumeWithException(exc)
            }

            override fun onConfigured(session: CameraCaptureSession) = cont.resume(session)

        }, cameraHandler)
    }

    /**
     * Helper function used to capture a still image using the [CameraDevice.TEMPLATE_STILL_CAPTURE]
     * template. It performs synchronization between the [CaptureResult] and the [Image] resulting
     * from the single capture, and outputs a [CombinedCaptureResult] object.
     */
    private suspend fun takePhoto():
                CombinedCaptureResult = suspendCoroutine { cont ->

        // Flush any images left in the image reader
        while (imageReader.acquireNextImage() != null) {}

        // Start a new image queue
        val imageQueue = ArrayBlockingQueue<Image>(IMAGE_BUFFER_SIZE)
        imageReader.setOnImageAvailableListener({ reader ->
            val image = reader.acquireNextImage()
            Log.d(TAG, "Image available in queue: ${image.timestamp}")
            imageQueue.add(image)
        }, imageReaderHandler)

        val captureRequest = session.device.createCaptureRequest(
                CameraDevice.TEMPLATE_STILL_CAPTURE).apply { addTarget(imageReader.surface) }
        setCameraSettings(captureRequest)

        // Display flash animation to indicate that photo was captured
        viewFinder.postDelayed({
            viewFinder.foreground = ColorDrawable(Color.WHITE)
            viewFinder.postDelayed(
                    { viewFinder.foreground = null }, ANIMATION_FAST_MILLIS)
        }, ANIMATION_SLOW_MILLIS)

        session.capture(captureRequest.build(), object : CameraCaptureSession.CaptureCallback() {
            override fun onCaptureCompleted(
                    session: CameraCaptureSession,
                    request: CaptureRequest,
                    result: TotalCaptureResult) {
                super.onCaptureCompleted(session, request, result)
                val resultTimestamp = result.get(CaptureResult.SENSOR_TIMESTAMP)
                Log.d(TAG, "Capture result received: $resultTimestamp")

                // Set a timeout in case image captured is dropped from the pipeline
                val exc = TimeoutException("Image dequeuing took too long")
                val timeoutRunnable = Runnable {
                    Log.e(TAG, exc.toString())
                    cont.resumeWithException(exc)
                }
                imageReaderHandler.postDelayed(timeoutRunnable, IMAGE_CAPTURE_TIMEOUT_MILLIS)

                // Loop in the coroutine's context until an image with matching timestamp comes
                // We need to launch the coroutine context again because the callback is done in
                //  the handler provided to the `capture` method, not in our coroutine context
                lifecycleScope.launch(cont.context) {
                    while (true) {
                        // Dequeue images while timestamps don't match
                        val image = imageQueue.take()
                        // TODO(owahltinez): b/142011420
                        // if (image.timestamp != resultTimestamp) continue
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q &&
                                image.format != ImageFormat.DEPTH_JPEG &&
                                image.timestamp != resultTimestamp) continue
                         Log.d(TAG, "Matching image dequeued: ${image.timestamp}")

                        // Unset the image reader listener
                        imageReaderHandler.removeCallbacks(timeoutRunnable)
                        imageReader.setOnImageAvailableListener(null, null)

                        // Clear the queue of images, if there are left
                        while (imageQueue.size > 0) {
                            imageQueue.take().close()
                        }

                        // Compute EXIF orientation metadata
                        val rotation = relativeOrientation.value ?: 0
                        val mirrored = characteristics.get(CameraCharacteristics.LENS_FACING) ==
                                CameraCharacteristics.LENS_FACING_FRONT
                        val exifOrientation = computeExifOrientation(rotation, mirrored)

                        // Build the result and resume progress
                        cont.resume(CombinedCaptureResult(
                                image, result, exifOrientation, imageReader.imageFormat))

                        // There is no need to break out of the loop, this coroutine will suspend
                        break;
                    }
                }
            }
        }, cameraHandler)
    }

    /** Helper function used to save a [CombinedCaptureResult] into a [File] */
//    private suspend fun saveResult(result: CombinedCaptureResult): File = suspendCoroutine { cont ->
//        when {
//
//            // When the format is JPEG or DEPTH JPEG we can simply save the bytes as-is
//            result.format == ImageFormat.JPEG || result.format == ImageFormat.DEPTH_JPEG -> {
//                val buffer = result.image.planes[0].buffer
//                val bytes = ByteArray(buffer.remaining()).apply { buffer.get(this) }
//                try {
//                    val output = createFile(requireContext(), "jpg")
//                    FileOutputStream(output).use { it.write(bytes) }
//                    cont.resume(output)
//                } catch (exc: IOException) {
//                    Log.e(TAG, "Unable to write RAW image to file", exc)
//                    cont.resumeWithException(exc)
//                }
//
//            }
//
//            // When the format is RAW we use the DngCreator utility library
//            result.format == ImageFormat.RAW_SENSOR -> {
//                val dngCreator = DngCreator(characteristics, result.metadata)
//                try {
//                    val output = createFile(requireContext(), "dng")
//                    FileOutputStream(output).use { dngCreator.writeImage(it, result.image) }
//                    cont.resume(output)
//                } catch (exc: IOException) {
//                    Log.e(TAG, "Unable to write DNG image to file", exc)
//                    cont.resumeWithException(exc)
//                }
//            }
//
//            // No other formats are supported by this sample
//            else -> {
//                val exc = RuntimeException("Unknown image format: ${result.image.format}")
//                Log.e(TAG, exc.message, exc)
//                cont.resumeWithException(exc)
//            }
//        }
//    }

    override fun onStop() {
        super.onStop()
        session.close()
        session.device.close()
    }

    override fun onDestroy() {
        super.onDestroy()
        cameraThread.quitSafely()
        imageReaderThread.quitSafely()
    }

    companion object {
        private val TAG = CameraFragment::class.java.simpleName

        /** Maximum number of images that will be held in the reader's buffer */
        private const val IMAGE_BUFFER_SIZE: Int = 1

        /** Maximum time allowed to wait for the result of an image capture */
        private const val IMAGE_CAPTURE_TIMEOUT_MILLIS: Long = 5000

        /** Helper data class used to pass around capture results with their associated image */
        data class CombinedCaptureResult(
                val image: Image,
                val metadata: CaptureResult,
                val orientation: Int,
                val format: Int
        ) : Closeable {
            override fun close() = image.close()
        }

        /**
         * Create a [File] named a using formatted timestamp with the current date and time.
         *
         * @return [File] created.
         */
        private fun createFile(context: Context, extension: String): File {
            val sdf = SimpleDateFormat("yyyy_MM_dd_HH_mm_ss_SSS", Locale.US)
            return File(context.filesDir, "IMG_${sdf.format(Date())}.$extension")
        }
    }
}
