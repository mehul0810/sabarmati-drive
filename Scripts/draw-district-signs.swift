import AppKit
import Foundation

// Original bilingual sign paintings. Bake the lettering to pixels, so a player
// can read the signs without installing a Gujarati typeface or connecting online.
guard CommandLine.arguments.count == 2 else { exit(2) }
let folder = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
let width = 1536, height = 288
func ink(_ r: Int, _ g: Int, _ b: Int) -> NSColor {
    NSColor(srgbRed: CGFloat(r)/255, green: CGFloat(g)/255, blue: CGFloat(b)/255, alpha: 1)
}
func draw(_ name: String, _ local: String, _ english: String, _ subline: String, _ base: NSColor) throws {
    guard let pixels = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: width, pixelsHigh: height,
        bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0),
        let ctx = NSGraphicsContext(bitmapImageRep: pixels) else { exit(3) }
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = ctx
    func rect(_ x: Int, _ y: Int, _ w: Int, _ h: Int, _ color: NSColor) {
        color.setFill()
        NSBezierPath(rect: NSRect(x:x,y:y,width:w,height:h)).fill()
    }
    let cream = ink(244,229,190), brass = ink(201,162,96)
    rect(0,0,width,height,base)
    rect(18,17,width-36,3,brass); rect(18,height-20,width-36,3,brass)
    func line(_ value: String, _ x: Int, _ y: Int, _ font: NSFont, _ color: NSColor, _ tracking: CGFloat) {
        (value as NSString).draw(at: NSPoint(x:x,y:y), withAttributes:[.font:font,.foregroundColor:color,.kern:tracking])
    }
    let gu = NSFont(name:"KohinoorGujarati-Semibold",size:101) ?? NSFont.systemFont(ofSize:101,weight:.semibold)
    let display = NSFont(name:"Georgia-Bold",size:74) ?? NSFont.systemFont(ofSize:74,weight:.bold)
    let small = NSFont.systemFont(ofSize:27,weight:.semibold)
    line(local,53,110,gu,cream,1)
    line(english,62,32,display,cream,3)
    rect(1095,53,3,181,brass)
    for (row, word) in subline.components(separatedBy:"|").enumerated() {
        line(word,1130,176 - row * 47,small,cream,2)
    }
    // Worn fixing heads and isolated pigment loss at the board edges.
    for x in [31, width-40] {
        for y in [31,height-41] {
            brass.setFill(); NSBezierPath(ovalIn:NSRect(x:x,y:y,width:9,height:9)).fill()
            rect(x+2,y+4,5,1,base)
        }
    }
    var seed: UInt32 = 17781
    func random(_ n: Int) -> Int { seed = seed &* 1664525 &+ 1013904223; return Int(seed >> 8) % n }
    for _ in 0..<950 {
        let x=random(width); let y=random(height)
        let a: CGFloat = y < 20 || y > height-20 ? 0.22 : 0.034
        rect(x,y,1+random(6),1,cream.withAlphaComponent(a))
    }
    ctx.flushGraphics(); NSGraphicsContext.restoreGraphicsState()
    try pixels.representation(using:.png,properties:[:])!.write(to:folder.appendingPathComponent(name + ".png"))
}
try draw("T_NadiChai", "નદી ચા ઘર", "NADI CHAI GHAR", "MASALA CHAI|FRESH MILK|SINCE 1984", ink(30,66,63))
try draw("T_KagazPrint", "કાગળ પ્રેસ", "KAGAZ PRESS", "LETTERPRESS|STATIONERY|BOOKBINDING", ink(99,50,39))
