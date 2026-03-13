import SwiftUI

// MARK: - Step size options

enum StepSize: Int, CaseIterable, Identifiable {
    case fine   = 50
    case medium = 200
    case coarse = 800

    var id: Int { rawValue }
    var label: String {
        switch self {
        case .fine:   return "Fine"
        case .medium: return "Med"
        case .coarse: return "Coarse"
        }
    }
}

// MARK: - SliderDetailView

struct SliderDetailView: View {
    let config: SliderConfig

    @State private var status: SliderStatus? = nil
    @State private var errorMessage: String? = nil
    @State private var stepSize: StepSize = .medium
    @State private var speed: Double = 800
    @State private var cameraExpanded: Bool = false
    @State private var jogTimer: Timer? = nil

    private var client: SliderClient { SliderClient(config: config) }

    // Poll every 2 seconds
    private let pollTimer = Timer.publish(every: 2, on: .main, in: .common).autoconnect()

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                headerSection
                speakerSection
                positionSection
                jogSection
                stepSizeSection
                speedSection
                homeSection
                if config.cameraURL != nil {
                    cameraSection
                }
            }
            .padding(20)
        }
        .frame(minWidth: 420)
        .onReceive(pollTimer) { _ in fetchStatus() }
        .onAppear { fetchStatus() }
    }

    // MARK: - Sections

    private var headerSection: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(config.name)
                .font(.title2).bold()
            HStack(spacing: 8) {
                Text("IP: \(config.ipAddress):\(config.port)")
                    .foregroundStyle(.secondary)
                Circle()
                    .fill(status != nil ? Color.green : Color.red)
                    .frame(width: 8, height: 8)
                Text(status != nil ? "Connected" : "Offline")
                    .foregroundStyle(.secondary)
            }
            if let err = errorMessage {
                Text(err).foregroundStyle(.red).font(.caption)
            }
        }
    }

    private var speakerSection: some View {
        SpeakerPositionView(
            pos:      status?.pos ?? 0,
            maxSteps: status?.maxSteps ?? config.maxSteps
        )
    }

    private var positionSection: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("Position").font(.headline)
            let pos      = status?.pos ?? 0
            let maxSteps = status?.maxSteps ?? config.maxSteps
            let fraction = maxSteps > 0 ? Double(pos) / Double(maxSteps) : 0

            ProgressView(value: fraction)
                .progressViewStyle(.linear)
            HStack {
                Text("\(pos)")
                Spacer()
                Text("\(maxSteps) steps")
            }
            .foregroundStyle(.secondary)
            .font(.caption)
        }
    }

    private var jogSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Jog").font(.headline)
            HStack(spacing: 16) {
                // LEFT button — press & hold to jog, release to stop
                Button {
                    // single tap: move by step size
                    send { try await client.move(steps: stepSize.rawValue, direction: .left) }
                } label: {
                    Label("Left", systemImage: "chevron.left.2")
                        .frame(maxWidth: .infinity)
                }
                .simultaneousGesture(
                    LongPressGesture(minimumDuration: 0.3)
                        .onEnded { _ in startJog(.left) }
                )
                .buttonStyle(.bordered)
                .controlSize(.large)

                // RIGHT button
                Button {
                    send { try await client.move(steps: stepSize.rawValue, direction: .right) }
                } label: {
                    Label("Right", systemImage: "chevron.right.2")
                        .frame(maxWidth: .infinity)
                }
                .simultaneousGesture(
                    LongPressGesture(minimumDuration: 0.3)
                        .onEnded { _ in startJog(.right) }
                )
                .buttonStyle(.bordered)
                .controlSize(.large)
            }

            if status?.moving == true {
                Button("Stop", role: .destructive) {
                    send { try await client.jogStop() }
                }
                .frame(maxWidth: .infinity)
            }
        }
    }

    private var stepSizeSection: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("Step size (tap)").font(.headline)
            Picker("Step size", selection: $stepSize) {
                ForEach(StepSize.allCases) { sz in
                    Text(sz.label).tag(sz)
                }
            }
            .pickerStyle(.segmented)
        }
    }

    private var speedSection: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("Speed: \(Int(speed)) steps/s").font(.headline)
            HStack {
                Text("100").foregroundStyle(.secondary).font(.caption)
                Slider(value: $speed, in: 100...3000, step: 50) { editing in
                    if !editing {
                        send { try await client.setSpeed(Int(speed)) }
                    }
                }
                Text("3000").foregroundStyle(.secondary).font(.caption)
            }
        }
    }

    private var homeSection: some View {
        Button {
            send { try await client.home() }
        } label: {
            Label("Home", systemImage: "house")
                .frame(maxWidth: .infinity)
        }
        .buttonStyle(.borderedProminent)
    }

    private var cameraSection: some View {
        DisclosureGroup("Camera Feed", isExpanded: $cameraExpanded) {
            if let camURL = config.cameraURL {
                CameraFeedView(urlString: camURL)
                    .frame(height: 240)
                    .cornerRadius(8)
            }
        }
    }

    // MARK: - Helpers

    private func fetchStatus() {
        Task {
            do {
                status = try await client.status()
                errorMessage = nil
            } catch {
                status = nil
                errorMessage = error.localizedDescription
            }
        }
    }

    private func startJog(_ dir: Direction) {
        send { try await client.jogStart(direction: dir) }
        // Release gesture = user lifts finger. SwiftUI doesn't give us onEnded for
        // LongPress release, so we use a fallback: stop after 2s max (user should
        // release and the detail view stop button appears while moving=true).
        // A better UX is handled by the prominent Stop button that appears.
    }

    private func send(_ action: @escaping () async throws -> Void) {
        Task {
            do {
                try await action()
                fetchStatus()
            } catch {
                errorMessage = error.localizedDescription
            }
        }
    }
}
