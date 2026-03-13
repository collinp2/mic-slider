import SwiftUI

/// Top-down speaker diagram showing mic position.
/// pos=0 → dead center of voice coil. pos=maxSteps → right edge (limit switch).
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
                // Background
                RoundedRectangle(cornerRadius: 10)
                    .fill(Color(nsColor: .windowBackgroundColor))

                Canvas { context, size in
                    let cx = size.width  / 2
                    let cy = size.height / 2
                    let maxR = min(cx, cy) - 12

                    // ── Speaker rings ─────────────────────────────────────
                    let ringCount = 6
                    for i in (1...ringCount).reversed() {
                        let r = maxR * Double(i) / Double(ringCount)
                        let rect = CGRect(x: cx - r, y: cy - r, width: r * 2, height: r * 2)
                        var p = Path(); p.addEllipse(in: rect)
                        let alpha = 0.15 + 0.12 * Double(ringCount - i + 1)
                        context.stroke(p, with: .color(.white.opacity(alpha)), lineWidth: i == ringCount ? 2.5 : 1.2)
                    }

                    // ── Voice coil ────────────────────────────────────────
                    let vcR = maxR * 0.14
                    let vcRect = CGRect(x: cx - vcR, y: cy - vcR, width: vcR * 2, height: vcR * 2)
                    var vcPath = Path(); vcPath.addEllipse(in: vcRect)
                    context.fill(vcPath, with: .color(.white.opacity(0.08)))
                    context.stroke(vcPath, with: .color(.white.opacity(0.5)), lineWidth: 1.5)

                    // ── Mic travel line ───────────────────────────────────
                    var linePath = Path()
                    linePath.move(to: CGPoint(x: cx, y: cy))
                    linePath.addLine(to: CGPoint(x: cx + maxR - 4, y: cy))
                    context.stroke(linePath, with: .color(.white.opacity(0.15)), lineWidth: 1)

                    // ── Mic indicator ──────────────────────────────────────
                    let micX = cx + fraction * (maxR - 4)
                    let micR: Double = 7

                    // Trail from center to mic
                    var trailPath = Path()
                    trailPath.move(to: CGPoint(x: cx, y: cy))
                    trailPath.addLine(to: CGPoint(x: micX, y: cy))
                    context.stroke(trailPath, with: .color(.red.opacity(0.5)), lineWidth: 2)

                    // Mic dot
                    let micRect = CGRect(x: micX - micR, y: cy - micR, width: micR * 2, height: micR * 2)
                    var micPath = Path(); micPath.addEllipse(in: micRect)
                    context.fill(micPath, with: .color(.red))
                    context.stroke(micPath, with: .color(.white.opacity(0.8)), lineWidth: 1.5)

                    // ── Center mark ───────────────────────────────────────
                    let cmR: Double = 3
                    let cmRect = CGRect(x: cx - cmR, y: cy - cmR, width: cmR * 2, height: cmR * 2)
                    var cmPath = Path(); cmPath.addEllipse(in: cmRect)
                    context.fill(cmPath, with: .color(.white.opacity(0.6)))
                }

                // Labels
                VStack {
                    Spacer()
                    HStack {
                        Text("center")
                            .font(.system(size: 10))
                            .foregroundStyle(.secondary)
                        Spacer()
                        Text("edge →")
                            .font(.system(size: 10))
                            .foregroundStyle(.secondary)
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
