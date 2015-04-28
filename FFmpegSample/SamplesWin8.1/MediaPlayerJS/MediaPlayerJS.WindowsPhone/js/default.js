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
    var appBar;
    var forceAudioDecode = false;
    var forceVideoDecode = false;

    app.onready = function (args) {
        // Register all event handler
        appBar = document.getElementById("topAppBar").winControl;
        appBar.getCommandById("cmdBrowse").addEventListener("click", doClickBrowse, false);
        appBar.getCommandById("cmdAudio").addEventListener("click", doClickAudio, false);
        appBar.getCommandById("cmdVideo").addEventListener("click", doClickVideo, false);

        mediaElement.addEventListener("ended", mediaElementEnded, false);
        mediaElement.addEventListener("error", mediaElementFailed, false);
    };

    // Handle the returned files from file picker
    function continueFileOpenPicker(args)
    {
        if (args.files !== undefined && args.files.size > 0) {
            mediaElement.pause();

            // Open StorageFile as IRandomAccessStream to be passed to FFmpegInteropMSS
            var file = args.files[0];
            file.openAsync(Windows.Storage.FileAccessMode.read).done(
                function (readStream) {

                    try {
                        // Instantiate FFmpeg object and pass the stream from opened file
                        var FFmpegMSS = FFmpegInterop.FFmpegInteropMSS.createFFmpegInteropMSSFromStream(readStream, forceAudioDecode, forceVideoDecode);
                        var mss = FFmpegMSS.getMediaStreamSource();

                        if (mss) {
                            // Pass MediaStreamSource to Media Element
                            mediaElement.src = URL.createObjectURL(mss, { oneTimeOnly: true });
                            mediaElement.play();
                        } else {
                            displayErrorMessage(); // mss
                        }

                    } catch (ex) {
                        displayErrorMessage(ex.message) // exception
                    }
                },
                displayErrorMessage // openAsync()
            );
        }
    }

    function doClickBrowse() {
        var filePicker = new Windows.Storage.Pickers.FileOpenPicker();
        filePicker.viewMode = Windows.Storage.Pickers.PickerViewMode.thumbnail;
        filePicker.suggestedStartLocation = Windows.Storage.Pickers.PickerLocationId.videosLibrary;
        filePicker.fileTypeFilter.replaceAll(["*"]);

        // Launch file picker that will be handled by ContinueFileOpenPicker after application is re-activated
        filePicker.pickSingleFileAndContinue();
    }

    function doClickAudio() {
        var button = appBar.getCommandById("cmdAudio");
        forceAudioDecode = button.selected;

        if (button.selected) {
            button.label = "Enable audio auto decoding";
        } else {
            button.label = "Disable audio auto decoding";
        }
    }

    function doClickVideo() {
        var button = appBar.getCommandById("cmdVideo");
        forceVideoDecode = button.selected;

        if (button.selected) {
            button.label = "Enable video auto decoding";
        } else {
            button.label = "Disable video auto decoding";
        }
    }

    function mediaElementEnded() {
    }

    function mediaElementFailed() {
        displayErrorMessage();
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
        } else if (args.detail.kind === activation.ActivationKind.pickFileContinuation) {
            continueFileOpenPicker(args.detail);
        }
    };

    app.oncheckpoint = function (args) {
    };

    app.start();

})();