(function () {
    var redirect = intellisense.redirectDefinition;

    var originalKeys = Object.keys;
    function baseKeys(o) {
        var values = Object.getOwnPropertyNames(o);
        for (var i = 0, len = values.length; i < len; ++i) {
            if (values[i].substr(0, 7) === "_$field") return values;
        }
        return originalKeys(o);
    }
    Object.keys = baseKeys;

    redirect(baseKeys, originalKeys);

    window._$originalAddEventListener = window.addEventListener;
    function addEventListener(type, handler, useCapture) {
        if (typeof (type) === "string" && (type === "pointerdown" || type === "keydown")) {
            handler = function () { };
        }
        return window._$originalAddEventListener(type, handler, useCapture);
    }
    window.addEventListener = addEventListener;

    redirect(addEventListener, window._$originalAddEventListener);
})();