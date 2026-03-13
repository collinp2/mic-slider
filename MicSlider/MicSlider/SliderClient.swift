import Foundation

// MARK: - Models

struct SliderStatus: Codable {
    let pos: Int
    let moving: Bool
    let maxSteps: Int
    let jogDir: String
    let homing: Bool
    let speed: Int
}

enum Direction: String {
    case left, right
}

// MARK: - SliderClient

/// Thin URLSession wrapper for the Arduino HTTP API.
actor SliderClient {
    let config: SliderConfig

    private var session: URLSession = {
        let cfg = URLSessionConfiguration.default
        cfg.timeoutIntervalForRequest = 3
        return URLSession(configuration: cfg)
    }()

    init(config: SliderConfig) {
        self.config = config
    }

    // MARK: Endpoints

    func status() async throws -> SliderStatus {
        let data = try await get("/status")
        return try JSONDecoder().decode(SliderStatus.self, from: data)
    }

    func move(steps: Int, direction: Direction) async throws {
        _ = try await get("/move?dir=\(direction.rawValue)&steps=\(steps)")
    }

    func jogStart(direction: Direction) async throws {
        _ = try await get("/jog/start?dir=\(direction.rawValue)")
    }

    func jogStop() async throws {
        _ = try await get("/jog/stop")
    }

    func home() async throws {
        _ = try await get("/home")
    }

    func setSpeed(_ stepsPerSec: Int) async throws {
        _ = try await get("/speed?val=\(stepsPerSec)")
    }

    // MARK: Private

    private func get(_ path: String) async throws -> Data {
        // Append slider channel to every request
        let sep = path.contains("?") ? "&" : "?"
        let fullPath = "\(path)\(sep)slider=\(config.channel)"
        guard let url = URL(string: "http://\(config.ipAddress):\(config.port)\(fullPath)") else {
            throw URLError(.badURL)
        }
        let (data, response) = try await session.data(from: url)
        guard let http = response as? HTTPURLResponse, (200..<300).contains(http.statusCode) else {
            throw URLError(.badServerResponse)
        }
        return data
    }
}
