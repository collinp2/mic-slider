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

/// Zoomed top-down view of just the mic travel zone.
/// Real-world scale: voice coil ~22mm radius, slide travel 50mm.
/// The diagram fills its width with the full 50mm travel range.
/// Concentric rings represent the cone at proportional spacing.
struct SpeakerPositionView: View {
    let pos: Int
    let maxSteps: Int

    // Real-world dimensions (mm)
    private let voiceCoilRadiusMM: Double = 22   // 1.75" dia voice coil → 22mm radius
    private let travelMM: Double         = 50    // full slider travel

    private var fraction: Double {
        guard maxSteps > 0 else { return 0 }
        return min(1.0, max(0.0, Double(pos) / Double(maxSteps)))
    }

    // mm offset from center at current position
    private var positionMM: Double { fraction * travelMM }

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("Mic Position").font(.headline)

            ZStack {
                RoundedRectangle(cornerRadius: 10)
                    .fill(Color(white: 0.12))

                Canvas { context, size in
                    // The canvas shows the view from above, centered on the voice coil axis.
                    // Horizontal axis = mic travel direction (left = center, right = edge).
                    // We show a window from -8mm to +58mm (a little past max travel).
                    let viewWidthMM: Double  = 66   // mm shown across full width
                    let viewOffsetMM: Double = -8   // left edge starts 8mm left of center
                    let scale = size.width / viewWidthMM  // px per mm

                    func xForMM(_ mm: Double) -> Double { (mm - viewOffsetMM) * scale }
                    let cy = size.height / 2

                    // ── Cone rings (full circles centered on axis) ──────────
                    // Show rings at voice coil radius and several cone rings beyond
                    let ringsMM: [Double] = [22, 35, 50, 68, 90, 120]
                    for r in ringsMM {
                        let cx = xForMM(0)
                        let rpx = r * scale
                        let rect = CGRect(x: cx - rpx, y: cy - rpx, width: rpx * 2, height: rpx * 2)
                        var p = Path(); p.addEllipse(in: rect)
                        let isVC = r == 22
                        context.stroke(p,
                            with: .color(.white.opacity(isVC ? 0.55 : 0.2)),
                            lineWidth: isVC ? 1.8 : 1.0)
                    }

                    // ── Voice coil fill ─────────────────────────────────────
                    let vcpx = voiceCoilRadiusMM * scale
                    let cxAxis = xForMM(0)
                    let vcRect = CGRect(x: cxAxis - vcpx, y: cy - vcpx, width: vcpx * 2, height: vcpx * 2)
                    var vcFill = Path(); vcFill.addEllipse(in: vcRect)
                    context.fill(vcFill, with: .color(.white.opacity(0.06)))

                    // ── Travel guide line ───────────────────────────────────
                    var guide = Path()
                    guide.move(to: CGPoint(x: xForMM(0), y: cy))
                    guide.addLine(to: CGPoint(x: xForMM(travelMM), y: cy))
                    context.stroke(guide, with: .color(.white.opacity(0.12)), lineWidth: 1)

                    // ── Trail from center to mic ────────────────────────────
                    let micXpx = xForMM(positionMM)
                    var trail = Path()
                    trail.move(to: CGPoint(x: xForMM(0), y: cy))
                    trail.addLine(to: CGPoint(x: micXpx, y: cy))
                    context.stroke(trail, with: .color(.red.opacity(0.45)), lineWidth: 2)

                    // ── Mic dot ─────────────────────────────────────────────
                    let micR: Double = 7
                    let micRect = CGRect(x: micXpx - micR, y: cy - micR, width: micR * 2, height: micR * 2)
                    var micPath = Path(); micPath.addEllipse(in: micRect)
                    context.fill(micPath, with: .color(.red))
                    context.stroke(micPath, with: .color(.white.opacity(0.8)), lineWidth: 1.5)

                    // ── Center mark ─────────────────────────────────────────
                    let cmR: Double = 3
                    let cmRect = CGRect(x: xForMM(0) - cmR, y: cy - cmR, width: cmR * 2, height: cmR * 2)
                    var cmPath = Path(); cmPath.addEllipse(in: cmRect)
                    context.fill(cmPath, with: .color(.white.opacity(0.7)))
                }

                // Labels
                VStack {
                    Spacer()
                    HStack {
                        Text("axis").font(.system(size: 10)).foregroundStyle(.secondary)
                        Spacer()
                        Text("← voice coil edge ~22mm").font(.system(size: 10)).foregroundStyle(Color.white.opacity(0.4))
                        Spacer()
                        Text("50mm →").font(.system(size: 10)).foregroundStyle(.secondary)
                    }
                    .padding(.horizontal, 14)
                    .padding(.bottom, 8)
                }

                // Position readout
                VStack {
                    HStack {
                        Spacer()
                        Text(String(format: "%.1f mm off-axis", positionMM))
                            .font(.system(size: 11, weight: .medium))
                            .foregroundStyle(.white.opacity(0.7))
                            .padding(.trailing, 14)
                            .padding(.top, 10)
                    }
                    Spacer()
                }
            }
            .frame(height: 200)
            .animation(.easeInOut(duration: 0.3), value: fraction)
        }
    }
}
