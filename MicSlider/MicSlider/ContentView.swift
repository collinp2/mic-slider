import SwiftUI

// MARK: - ContentView (sidebar + detail)

struct ContentView: View {
    @StateObject private var store = SliderStore()
    @State private var selectedID: UUID? = nil
    @State private var showAddSheet = false

    private static let lastSelectedKey = "MicSliderLastSelected"

    var body: some View {
        NavigationSplitView {
            List(selection: $selectedID) {
                ForEach(store.sliders) { slider in
                    NavigationLink(value: slider.id) {
                        Label(slider.name, systemImage: "slider.horizontal.3")
                    }
                }
                .onDelete { store.remove(at: $0) }
            }
            .navigationTitle("Sliders")
            .toolbar {
                ToolbarItem(placement: .primaryAction) {
                    Button {
                        showAddSheet = true
                    } label: {
                        Label("Add Slider", systemImage: "plus")
                    }
                }
            }
        } detail: {
            if let id = selectedID,
               let config = store.sliders.first(where: { $0.id == id }) {
                SliderDetailView(config: config)
                    .id(id) // force recreation when selection changes
            } else {
                VStack(spacing: 12) {
                    Image(systemName: "slider.horizontal.3")
                        .font(.system(size: 48))
                        .foregroundStyle(.tertiary)
                    Text("Select a slider")
                        .foregroundStyle(.secondary)
                    Button("Add Slider") { showAddSheet = true }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
        .sheet(isPresented: $showAddSheet) {
            AddSliderSheet { newConfig in
                store.add(newConfig)
                selectedID = newConfig.id
            }
        }
        .onAppear {
            // Restore last selected slider
            if let uuidString = UserDefaults.standard.string(forKey: Self.lastSelectedKey),
               let uuid = UUID(uuidString: uuidString),
               store.sliders.contains(where: { $0.id == uuid }) {
                selectedID = uuid
            } else {
                selectedID = store.sliders.first?.id
            }
        }
        .onChange(of: selectedID) { newID in
            UserDefaults.standard.set(newID?.uuidString, forKey: Self.lastSelectedKey)
        }
    }
}

// MARK: - AddSliderSheet

struct AddSliderSheet: View {
    var onAdd: (SliderConfig) -> Void
    @Environment(\.dismiss) private var dismiss

    @State private var name      = ""
    @State private var ip        = ""
    @State private var port      = "8080"
    @State private var cameraURL = ""
    @State private var maxSteps  = "40000"

    var body: some View {
        VStack(alignment: .leading, spacing: 20) {
            Text("Add Slider").font(.title2).bold()

            Form {
                TextField("Name", text: $name, prompt: Text("Stage Left Mic"))
                TextField("IP Address", text: $ip, prompt: Text("10.0.0.126"))
                TextField("Port", text: $port)
                TextField("Camera URL (optional)", text: $cameraURL, prompt: Text("http://192.168.1.50/stream"))
                TextField("Max Steps", text: $maxSteps)
            }
            .formStyle(.grouped)

            HStack {
                Spacer()
                Button("Cancel", role: .cancel) { dismiss() }
                Button("Add") {
                    let config = SliderConfig(
                        name:      name.isEmpty ? "Slider" : name,
                        ipAddress: ip,
                        port:      Int(port) ?? 80,
                        cameraURL: cameraURL.isEmpty ? nil : cameraURL,
                        maxSteps:  Int(maxSteps) ?? 20000
                    )
                    onAdd(config)
                    dismiss()
                }
                .buttonStyle(.borderedProminent)
                .disabled(ip.isEmpty)
            }
        }
        .padding(24)
        .frame(width: 400)
    }
}
