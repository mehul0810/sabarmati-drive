import AppKit

// Unreal uses this native bitmap before the game viewport is available.
let width = 860
let height = 430
let image = NSImage(size: NSSize(width: width, height: height), flipped: true) { rect in
    func color(_ hex: UInt32) -> NSColor {
        NSColor(srgbRed: CGFloat((hex >> 16) & 255) / 255,
                green: CGFloat((hex >> 8) & 255) / 255,
                blue: CGFloat(hex & 255) / 255, alpha: 1)
    }
    func label(_ s: String, x: CGFloat, y: CGFloat, font: NSFont, rgb: UInt32, spacing: CGFloat = 0) {
        (s as NSString).draw(at: CGPoint(x: x, y: y), withAttributes: [
            .font: font, .foregroundColor: color(rgb), .kern: spacing
        ])
    }
    NSGradient(starting: color(0x141a23), ending: color(0x243346))?.draw(in: rect, angle: 8)

    // Abstract lines suggest the shape of the canal district.
    for i in 0..<13 {
        let n = CGFloat(i) * 20
        let line = NSBezierPath()
        line.move(to: CGPoint(x: 405 + n, y: -20))
        line.curve(to: CGPoint(x: 876 + n / 2, y: 444),
                   controlPoint1: CGPoint(x: 750 + 0.4 * n, y: 91),
                   controlPoint2: CGPoint(x: 424 + n, y: 312))
        line.lineWidth = (i == 5 || i == 8) ? 2.1 : 1
        color(i == 5 ? 0xda786b : i == 8 ? 0x5ba7a3 : 0x3d505b).setStroke()
        line.stroke()
    }
    color(0xda786b).setFill()
    NSBezierPath(rect: NSRect(x: 55, y: 68, width: 39, height: 3)).fill()
    label("Grand City", x: 51, y: 114, font: .systemFont(ofSize: 73, weight: .heavy), rgb: 0xf4ecdf)

    color(0x5ba7a3).setFill()
    NSBezierPath(ovalIn: NSRect(x: 56, y: 327, width: 7, height: 7)).fill()
    label("Loading the city and your local profile…", x: 73, y: 319,
          font: .systemFont(ofSize: 19, weight: .semibold), rgb: 0xf4ecdf)
    return true
}
guard CommandLine.arguments.count == 2, let tiff = image.tiffRepresentation,
      let bitmap = NSBitmapImageRep(data: tiff), let data = bitmap.representation(using: .png, properties: [:]) else {
    fatalError("Usage: swift Scripts/make-mac-splash.swift Content/Splash/Splash.png")
}
try data.write(to: URL(fileURLWithPath: CommandLine.arguments[1]), options: .atomic)
