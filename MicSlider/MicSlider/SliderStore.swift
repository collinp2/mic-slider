import Foundation
import Combine

// MARK: - SliderConfig

struct SliderConfig: Identifiable, Codable, Hashable {
    var id = UUID()
    var name: String
    var ipAddress: String
    var port: Int = 80
    var cameraURL: String? = nil
    var maxSteps: Int = 20000
}

// MARK: - SliderStore

@MainActor
final class SliderStore: ObservableObject {
    @Published var sliders: [SliderConfig] = []

    private static let defaultsKey = "MicSliderConfigs"

    init() {
        load()
    }

    func add(_ config: SliderConfig) {
        sliders.append(config)
        save()
    }

    func remove(at offsets: IndexSet) {
        sliders.remove(atOffsets: offsets)
        save()
    }

    func update(_ config: SliderConfig) {
        guard let idx = sliders.firstIndex(where: { $0.id == config.id }) else { return }
        sliders[idx] = config
        save()
    }

    // MARK: Persistence

    private func save() {
        guard let data = try? JSONEncoder().encode(sliders) else { return }
        UserDefaults.standard.set(data, forKey: Self.defaultsKey)
    }

    private func load() {
        guard
            let data = UserDefaults.standard.data(forKey: Self.defaultsKey),
            let decoded = try? JSONDecoder().decode([SliderConfig].self, from: data)
        else { return }
        sliders = decoded
    }
}
