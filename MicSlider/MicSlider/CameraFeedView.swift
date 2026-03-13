import SwiftUI
import WebKit

/// Displays a live MJPEG or HLS camera stream in a WKWebView.
struct CameraFeedView: NSViewRepresentable {
    let urlString: String

    func makeNSView(context: Context) -> WKWebView {
        let wv = WKWebView()
        wv.allowsBackForwardNavigationGestures = false
        load(into: wv)
        return wv
    }

    func updateNSView(_ nsView: WKWebView, context: Context) {
        load(into: nsView)
    }

    private func load(into wv: WKWebView) {
        if let url = URL(string: urlString) {
            // For MJPEG streams, wrap in a minimal HTML page so the browser
            // handles the multipart response correctly.
            let html = """
            <!DOCTYPE html>
            <html>
            <head>
            <style>
              body { margin: 0; background: black; display: flex; align-items: center; justify-content: center; height: 100vh; }
              img { max-width: 100%; max-height: 100vh; object-fit: contain; }
            </style>
            </head>
            <body>
              <img src="\(url.absoluteString)" />
            </body>
            </html>
            """
            wv.loadHTMLString(html, baseURL: nil)
        }
    }
}
