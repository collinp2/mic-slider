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

// MARK: - SpeakerPositionView

/// Top-down speaker diagram. pos=0 = dead center of voice coil, pos=maxSteps = edge.
struct SpeakerPositionView: View {
    let pos: Int
    let maxSteps: Int

    private var fraction: Double {
        guard maxSteps > 0 else { return 0 }
        return min(1.0, max(0.0, Double(pos) / Double(maxSteps)))
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("Mic Position").font(.headline)

            ZStack {
                RoundedRectangle(cornerRadius: 10)
                    .fill(Color(nsColor: .windowBackgroundColor))

                Canvas { context, size in
                    let cx = size.width  / 2
                    let cy = size.height / 2
                    let maxR = min(cx, cy) - 12

                    // Speaker rings
                    let ringCount = 6
                    for i in (1...ringCount).reversed() {
                        let r = maxR * Double(i) / Double(ringCount)
                        let rect = CGRect(x: cx - r, y: cy - r, width: r * 2, height: r * 2)
                        var p = Path(); p.addEllipse(in: rect)
                        let alpha = 0.15 + 0.12 * Double(ringCount - i + 1)
                        context.stroke(p, with: .color(.white.opacity(alpha)), lineWidth: i == ringCount ? 2.5 : 1.2)
                    }

                    // Voice coil
                    let vcR = maxR * 0.14
                    let vcRect = CGRect(x: cx - vcR, y: cy - vcR, width: vcR * 2, height: vcR * 2)
                    var vcPath = Path(); vcPath.addEllipse(in: vcRect)
                    context.fill(vcPath, with: .color(.white.opacity(0.08)))
                    context.stroke(vcPath, with: .color(.white.opacity(0.5)), lineWidth: 1.5)

                    // Travel guide line
                    var linePath = Path()
                    linePath.move(to: CGPoint(x: cx, y: cy))
                    linePath.addLine(to: CGPoint(x: cx + maxR - 4, y: cy))
                    context.stroke(linePath, with: .color(.white.opacity(0.15)), lineWidth: 1)

                    // Trail from center to mic
                    let micX = cx + fraction * (maxR - 4)
                    var trailPath = Path()
                    trailPath.move(to: CGPoint(x: cx, y: cy))
                    trailPath.addLine(to: CGPoint(x: micX, y: cy))
                    context.stroke(trailPath, with: .color(.red.opacity(0.5)), lineWidth: 2)

                    // Mic dot
                    let micR: Double = 7
                    let micRect = CGRect(x: micX - micR, y: cy - micR, width: micR * 2, height: micR * 2)
                    var micPath = Path(); micPath.addEllipse(in: micRect)
                    context.fill(micPath, with: .color(.red))
                    context.stroke(micPath, with: .color(.white.opacity(0.8)), lineWidth: 1.5)

                    // Center mark
                    let cmR: Double = 3
                    let cmRect = CGRect(x: cx - cmR, y: cy - cmR, width: cmR * 2, height: cmR * 2)
                    var cmPath = Path(); cmPath.addEllipse(in: cmRect)
                    context.fill(cmPath, with: .color(.white.opacity(0.6)))
                }

                VStack {
                    Spacer()
                    HStack {
                        Text("center").font(.system(size: 10)).foregroundStyle(.secondary)
                        Spacer()
                        Text("edge →").font(.system(size: 10)).foregroundStyle(.secondary)
                    }
                    .padding(.horizontal, 14)
                    .padding(.bottom, 8)
                }
            }
            .frame(height: 180)
            .animation(.easeInOut(duration: 0.3), value: fraction)
        }
    }
}
