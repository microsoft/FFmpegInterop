//*****************************************************************************
//
//	Copyright 2015 Microsoft Corporation
//
//	Licensed under the Apache License, Version 2.0 (the "License");
//	you may not use this file except in compliance with the License.
//	You may obtain a copy of the License at
//
//	http ://www.apache.org/licenses/LICENSE-2.0
//
//	Unless required by applicable law or agreed to in writing, software
//	distributed under the License is distributed on an "AS IS" BASIS,
//	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//	See the License for the specific language governing permissions and
//	limitations under the License.
//
//*****************************************************************************
(function () {
	"use strict";

	var app = WinJS.Application;
	var splitView;
	var uriBox;
	var uriBoxPrompt = "Enter URI and press Enter";

	app.onready = function (args) {
	    // Initialize all UI control
	    splitView = document.getElementById("splitter").winControl;
	    document.getElementById("openFileButton").addEventListener("click", openLocalFile, false);

	    uriBox = document.getElementById("uriBox");
	    uriBox.value = uriBoxPrompt;
	    uriBox.addEventListener("focusin", uriBoxFocusIn, false);
	    uriBox.addEventListener("focusout", uriBoxFocusOut, false);
	    uriBox.addEventListener("keyup", uriBoxKeyUp, false);
	};

	function openLocalFile() {
	    var filePicker = new Windows.Storage.Pickers.FileOpenPicker();
	    filePicker.viewMode = Windows.Storage.Pickers.PickerViewMode.thumbnail;
	    filePicker.suggestedStartLocation = Windows.Storage.Pickers.PickerLocationId.videosLibrary;
	    filePicker.fileTypeFilter.replaceAll(["*"]);

	    // Show file picker so user can select a file
	    filePicker.pickSingleFileAsync().then(
            function (file) {
                if (file) {
                    mediaElement.pause();

                    // Open StorageFile as IRandomAccessStream to be passed to FFmpegInteropMSS
                    file.openAsync(Windows.Storage.FileAccessMode.read).done(
                        function (readStream) {

                            try {
                                // Read toggle switches states and use them to setup FFmpeg MSS
                                var forceDecodeAudio = document.getElementById("toggleSwitchAudioDecode").winControl.checked;
                                var forceDecodeVideo = document.getElementById("toggleSwitchVideoDecode").winControl.checked;

                                // Instantiate FFmpeg object and pass the stream from opened file
                                var FFmpegMSS = FFmpegInterop.FFmpegInteropMSS.createFFmpegInteropMSSFromStream(readStream, forceDecodeAudio, forceDecodeVideo);
                                var mss = FFmpegMSS.getMediaStreamSource();

                                if (mss) {
                                    // Pass MediaStreamSource to Media Element
                                    mediaElement.src = URL.createObjectURL(mss, { oneTimeOnly: true });
                                    mediaElement.play();

                                    // Close control panel after file open
                                    splitView.closePane();

                                } else {
                                    displayErrorMessage(); // mss
                                }

                            } catch (ex) {
                                displayErrorMessage(ex.message); // exception
                            }
                        },
                        displayErrorMessage // openAsync()
                    );
                }
            },
            displayErrorMessage // pickSingleFileAsync()
        );
	}

	function uriBoxKeyUp(event) {
	    // Only respond when the text box is not empty and after Enter key is pressed
	    if (event.keyCode == 13 && uriBox.value !== "") {
	        try {
	            // Read toggle switches states and use them to setup FFmpeg MSS
	            var forceDecodeAudio = document.getElementById("toggleSwitchAudioDecode").winControl.checked;
	            var forceDecodeVideo = document.getElementById("toggleSwitchVideoDecode").winControl.checked;

	            // Set FFmpeg specific options. List of options can be found in https://www.ffmpeg.org/ffmpeg-protocols.html
	            var options = new Windows.Foundation.Collections.PropertySet();

	            // Below are some sample options that you can set to configure RTSP streaming
	            // options.insert("rtsp_flags", "prefer_tcp");
	            // options.insert("stimeout", 100000);

	            // Instantiate FFmpegInteropMSS using the URI
	            mediaElement.pause();
	            var FFmpegMSS = FFmpegInterop.FFmpegInteropMSS.createFFmpegInteropMSSFromUri(uriBox.value, forceDecodeAudio, forceDecodeVideo, options);

	            if (FFmpegMSS) {
	                var mss = FFmpegMSS.getMediaStreamSource();

	                if (mss) {
	                    // Pass MediaStreamSource to Media Element
	                    mediaElement.src = URL.createObjectURL(mss, { oneTimeOnly: true });
	                    mediaElement.play();

	                    // Close control panel after file open
	                    splitView.closePane();

	                } else {
	                    displayErrorMessage(); // mss
	                }

	            } else {
	                displayErrorMessage("Cannot open URI: " + uriBox.value ); // FFmpegMSS
	            }

	        } catch (ex) {
	            displayErrorMessage(ex.message); // exception
	        }
	    }
	}

	function uriBoxFocusIn() {
        // Clear the URI text box when user select it while it contains no value
	    if (uriBox.value === uriBoxPrompt) {
	        uriBox.value = "";
	    }
	}

	function uriBoxFocusOut() {
	    // Put the prompt in the URI text box when user leave the URI box empty
	    if (uriBox.value === "") {
	        uriBox.value = uriBoxPrompt;
	    }
	}

	function displayErrorMessage(message) {
	    // Display error message
	    var errorMessage = !!message ? message : "Cannot open media";
	    var errorDialog = new Windows.UI.Popups.MessageDialog(errorMessage);
	    errorDialog.showAsync().done();
	}

    // App activation logic
	var activation = Windows.ApplicationModel.Activation;

	app.onactivated = function (args) {
	    if (args.detail.kind === activation.ActivationKind.launch) {
	        args.setPromise(WinJS.UI.processAll());
	    }
	};

	app.oncheckpoint = function (args) {
	};

	app.start();
})();