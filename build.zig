const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "pangowin32tobmp",
        .target = target,
        .optimize = optimize,
    });
    exe.addCSourceFiles(.{
        .files = &.{"examples/pangowin32tobmp.c"},
    });
    b.installArtifact(exe);

    const fribidi = buildFribidi(b, target, optimize);
    const glib_dep = b.dependency("glib", .{
        .target = target,
        .optimize = optimize,
    });
    const glib = glib_dep.artifact("glib");
    const gobject = glib_dep.artifact("gobject");
    const gio = glib_dep.artifact("gio");
    const gmodule = glib_dep.artifact("gmodule");
    const harfbuzz = buildHarfbuzz(b, target, optimize);
    b.installArtifact(harfbuzz);
    const freetype = buildFreetype(b, target, optimize);
    const fontconfig = buildFontconfig(b, target, optimize);
    fontconfig.linkLibrary(freetype);
    const expat = buildExpat(b, target, optimize);
    fontconfig.linkLibrary(expat);
    const pixman = buildPixman(b, target, optimize);
    const cairo = buildCairo(b, target, optimize);
    b.installArtifact(cairo);
    cairo.linkLibrary(pixman);
    cairo.linkLibrary(fontconfig);
    cairo.linkLibrary(freetype);
    const zlib = buildZlib(b, target, optimize);
    cairo.linkLibrary(zlib);

    const pango = buildPango(b, target, optimize);
    b.installArtifact(pango);
    pango.linkLibrary(fribidi);
    pango.linkLibrary(glib);
    pango.addIncludePath(glib.getEmittedIncludeTree().path(b, "glib"));
    pango.linkLibrary(gio);
    pango.linkLibrary(gmodule);
    pango.addIncludePath(gmodule.getEmittedIncludeTree().path(b, "gmodule"));
    pango.linkLibrary(gobject);
    pango.linkLibrary(harfbuzz);
    pango.linkLibrary(freetype);
    pango.linkLibrary(cairo);
    pango.linkLibrary(fontconfig);

    exe.linkLibrary(pango);
    exe.addIncludePath(pango.getEmittedIncludeTree().path(b, "pango"));
    exe.addIncludePath(b.path("generated"));
    exe.linkLibrary(glib);
    exe.addIncludePath(glib.getEmittedIncludeTree().path(b, "glib"));
    exe.linkLibrary(gobject);
    exe.linkLibrary(harfbuzz);
    exe.linkSystemLibrary("gdi32");
}

fn buildExpat(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep = b.dependency("expat", .{});
    const lib = b.addStaticLibrary(.{
        .name = "expat",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    lib.addIncludePath(b.path("generated/expat"));
    lib.addIncludePath(dep.path(""));
    lib.addCSourceFiles(.{
        .root = dep.path("expat/lib"),
        .files = &.{
            // "xmlparse.c",
            "xmlrole.c",
            "xmltok.c",
        },
        .flags = &.{
            "-DHAVE_EXPAT_CONFIG_H",
            "-std=gnu11",
        },
    });
    lib.installHeadersDirectory(dep.path("expat/lib"), "", .{});
    return lib;
}

fn buildPango(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const lib = b.addStaticLibrary(.{
        .name = "pango",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    lib.addIncludePath(b.path(""));
    lib.addIncludePath(b.path("pango"));
    lib.addIncludePath(b.path("generated"));
    lib.addIncludePath(b.path("generated/pango"));

    const flags = [_][]const u8{
        "-DG_LOG_DOMAIN=\"Pango\"",
        "-DG_LOG_USE_STRUCTURED=1",
        "-DPANGO_COMPILATION",
        "-DSYSCONFDIR=\"\"",
        "-DLIBDIR=\"\"",
    };
    lib.addCSourceFiles(.{
        .files = &.{
            "generated/pango/pango-enum-types.c",
        },
        .flags = &flags,
    });
    lib.addCSourceFiles(.{
        .root = b.path("pango"),
        .files = &PANGO_SRCS,
        .flags = &flags,
    });
    lib.installHeadersDirectory(b.path("pango"), "pango", .{});
    lib.installHeader(b.path("generated/pango/pango-features.h"), "pango/pango-features.h");
    lib.installHeader(b.path("generated/pango/pango-enum-types.h"), "pango/pango-enum-types.h");
    lib.linkSystemLibrary("Dwrite");

    return lib;
}

fn buildFreetype(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep = b.dependency("freetype", .{});
    const lib = b.addStaticLibrary(.{
        .name = "freetype",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    const flags = [_][]const u8{
        "-DFT2_BUILD_LIBRARY=1",
        "-DFT_CONFIG_OPTIONS_H=<ftoption.h>",
    };
    lib.addCSourceFiles(.{
        .root = dep.path(""),
        .files = &.{
            "src/base/ftbase.c",
            "src/base/ftinit.c",
            "builds/windows/ftsystem.c",
            "builds/windows/ftdebug.c",
        },
        .flags = &flags,
    });
    lib.addIncludePath(dep.path("include"));
    lib.addIncludePath(dep.path("include/freetype/config"));
    lib.installHeadersDirectory(dep.path("include"), "", .{});
    return lib;
}

fn buildPixman(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep = b.dependency("pixman", .{});
    const lib = b.addStaticLibrary(.{
        .name = "pixman",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    lib.addIncludePath(dep.path("src"));
    lib.addIncludePath(b.path("generated/pixman"));
    lib.addCSourceFiles(.{
        .root = dep.path("pixman"),
        .files = &PIXMAN_SRCS,
        .flags = &.{
            "-DHAVE_CONFIG_H",
        },
    });
    lib.installHeader(dep.path("pixman/pixman.h"), "pixman.h");
    lib.installHeader(b.path("generated/pixman/pixman-version.h"), "pixman-version.h");
    return lib;
}

fn buildCairo(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep = b.dependency("cairo", .{});
    const lib = b.addStaticLibrary(.{
        .name = "cairo",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    lib.addIncludePath(dep.path("src"));
    lib.addIncludePath(b.path("generated/cairo"));
    lib.addCSourceFiles(.{
        .root = dep.path("src"),
        .files = &CAIRO_SRCS,
        .flags = &.{
            "-D_FILE_OFFSET_BITS=64",
            "-D_FORTIFY_SOURCE=2",
            "-D_GNU_SOURCE",
            "-DWIN32_LEAN_AND_MEAN",
            "-DNOMINMAX",
            "-DWINVER=_WIN32_WINNT_WIN10",
            "-D_WIN32_WINNT=_WIN32_WINNT_WIN10",
            "-DNTDDI_VERSION=NTDDI_WIN10_RS3",
            "-DCAIRO_WIN32_STATIC_BUILD",
            "-D_REENTRANT",
            "-DCAIRO_COMPILATION",
            "-Wno-attributes",
            "-Wno-unused-butset-variable",
            "-Wno-unused-result",
            "-Wno-missing-field-initializers",
            "-Wno-unused-parameter",
            "-Wno-long-long",
            "-Wno-macro-redefined",
        },
    });
    lib.installHeadersDirectory(dep.path("src"), "", .{});
    lib.installHeader(b.path("generated/cairo/cairo-features.h"), "cairo-features.h");
    return lib;
}

fn buildZlib(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep = b.dependency("zlib", .{});
    const lib = b.addStaticLibrary(.{
        .name = "zlib",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    lib.addIncludePath(dep.path(""));
    lib.installHeader(dep.path("zconf.h"), "zconf.h");
    lib.installHeader(dep.path("zlib.h"), "zlib.h");
    lib.addCSourceFiles(.{
        .root = dep.path(""),
        .files = &.{
            "adler32.c",
            "compress.c",
            "crc32.c",
            "deflate.c",
            "gzclose.c",
            "gzlib.c",
            "gzread.c",
            "gzwrite.c",
            "inflate.c",
            "infback.c",
            "inftrees.c",
            "inffast.c",
            "trees.c",
            "uncompr.c",
            "zutil.c",
        },
    });
    return lib;
}

fn buildHarfbuzz(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep = b.dependency("harfbuzz", .{});
    const lib = b.addStaticLibrary(.{
        .name = "harfbuzz",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    lib.addCSourceFiles(.{
        .root = dep.path("src"),
        .files = &HB_SRCS,
        .flags = &.{
            "-DHAVE_DIRECTWRITE",
        },
    });
    lib.linkLibCpp();
    // lib.installHeader(dep.path("src/hb.h"), "hb.h");
    lib.installHeadersDirectory(dep.path("src"), "", .{});
    return lib;
}

fn buildFontconfig(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep = b.dependency("fontconfig", .{});
    const lib = b.addStaticLibrary(.{
        .name = "fontconfig",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    lib.addIncludePath(b.path("generated/fontconfig"));
    lib.installHeadersDirectory(dep.path("fontconfig"), "fontconfig", .{});
    lib.addIncludePath(dep.path(""));
    lib.addIncludePath(dep.path("src"));
    lib.addCSourceFiles(.{
        .root = dep.path("src"),
        .files = &FC_SRCS,
        .flags = &.{
            "-DHAVE_CONFIG_H",
        },
    });

    {
        const run = b.addSystemCommand(&.{"py"});
        run.addFileArg(dep.path("src/makealias.py"));
        run.addDirectoryArg(dep.path("src"));
        //
        _ = run.addOutputFileArg("fcalias.h");
        const fcaliastail_h = run.addOutputFileArg("fcaliastail.h");
        lib.addIncludePath(fcaliastail_h.dirname());
        run.addFileArg(dep.path("fontconfig/fontconfig.h"));
        run.addFileArg(dep.path("src/fcdeprecate.h"));
        run.addFileArg(dep.path("fontconfig/fcprivate.h"));
        // lib.step.dependOn(&run.step);
    }

    {
        const run = b.addSystemCommand(&.{"py"});
        run.addFileArg(dep.path("src/makealias.py"));
        run.addDirectoryArg(dep.path("src"));
        //
        _ = run.addOutputFileArg("fcftalias.h");
        const fcftaliastail_h = run.addOutputFileArg("fcftaliastail.h");
        lib.addIncludePath(fcftaliastail_h.dirname());
        run.addFileArg(dep.path("fontconfig/fcfreetype.h"));
        // lib.step.dependOn(&run.step);
    }

    return lib;
}

const FC_SRCS = [_][]const u8{
    "fcatomic.c",
    // "fccache.c",
    // "fccfg.c",
    // "fccharset.c",
    // "fccompat.c",
    // "fcdbg.c",
    // "fcdefault.c",
    // "fcdir.c",
    // "fcformat.c",
    // "fcfreetype.c",
    // "fcfs.c",
    // "fcptrlist.c",
    // "fchash.c",
    // "fcinit.c",
    // // "fclang.c",
    // "fclist.c",
    // "fcmatch.c",
    // "fcmatrix.c",
    // "fcname.c",
    // "fcobjs.c",
    // "fcpat.c",
    // "fcrange.c",
    // "fcserialize.c",
    // "fcstat.c",
    // "fcstr.c",
    // "fcweight.c",
    // "fcxml.c",
    // "ftglue.c",
};

fn buildFribidi(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const dep = b.dependency("fribidi", .{});
    const lib = b.addStaticLibrary(.{
        .name = "fribidi",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    lib.addCSourceFiles(.{
        .root = dep.path("lib"),
        .files = &.{
            "fribidi.c",
            "fribidi-arabic.c",
            "fribidi-bidi.c",
            "fribidi-bidi-types.c",
            "fribidi-char-sets.c",
            "fribidi-char-sets-cap-rtl.c",
            "fribidi-char-sets-cp1255.c",
            "fribidi-char-sets-cp1256.c",
            "fribidi-char-sets-iso8859-6.c",
            "fribidi-char-sets-iso8859-8.c",
            "fribidi-char-sets-utf8.c",
            "fribidi-deprecated.c",
            "fribidi-joining.c",
            "fribidi-joining-types.c",
            "fribidi-mirroring.c",
            "fribidi-brackets.c",
            "fribidi-run.c",
            "fribidi-shape.c",
        },
        .flags = &.{
            "-DHAVE_CONFIG_H",
            "-DFRIBIDI_LIB_STATIC",
        },
    });
    lib.addIncludePath(dep.path("lib"));
    lib.addIncludePath(b.path("generated/fribidi"));
    lib.installHeadersDirectory(dep.path("lib"), "", .{});
    lib.installHeader(b.path("generated/fribidi/fribidi-config.h"), "fribidi-config.h");
    lib.installHeader(b.path("generated/fribidi/fribidi-unicode-version.h"), "fribidi-unicode-version.h");

    return lib;
}

const HB_SRCS = [_][]const u8{
    "hb-aat-layout.cc",
    "hb-aat-map.cc",
    "hb-blob.cc",
    "hb-buffer-serialize.cc",
    "hb-buffer-verify.cc",
    "hb-buffer.cc",
    "hb-common.cc",
    "hb-draw.cc",
    "hb-paint.cc",
    "hb-paint-extents.cc",
    "hb-face.cc",
    "hb-face-builder.cc",
    "hb-fallback-shape.cc",
    "hb-font.cc",
    "hb-map.cc",
    "hb-number.cc",
    "hb-ot-cff1-table.cc",
    "hb-ot-cff2-table.cc",
    "hb-ot-color.cc",
    "hb-ot-face.cc",
    "hb-ot-font.cc",
    "hb-outline.cc",
    "hb-ot-layout.cc",
    "hb-ot-map.cc",
    "hb-ot-math.cc",
    "hb-ot-meta.cc",
    "hb-ot-metrics.cc",
    "hb-ot-name.cc",
    "hb-ot-shaper-arabic.cc",
    "hb-ot-shaper-default.cc",
    "hb-ot-shaper-hangul.cc",
    "hb-ot-shaper-hebrew.cc",
    "hb-ot-shaper-indic-table.cc",
    "hb-ot-shaper-indic.cc",
    "hb-ot-shaper-khmer.cc",
    "hb-ot-shaper-myanmar.cc",
    "hb-ot-shaper-syllabic.cc",
    "hb-ot-shaper-thai.cc",
    "hb-ot-shaper-use.cc",
    "hb-ot-shaper-vowel-constraints.cc",
    "hb-ot-shape-fallback.cc",
    "hb-ot-shape-normalize.cc",
    "hb-ot-shape.cc",
    "hb-ot-tag.cc",
    "hb-ot-var.cc",
    "hb-set.cc",
    "hb-shape-plan.cc",
    "hb-shape.cc",
    "hb-shaper.cc",
    "hb-static.cc",
    "hb-style.cc",
    "hb-ucd.cc",
    "hb-unicode.cc",
    //
    "hb-directwrite.cc",
};

const PANGO_SRCS = [_][]const u8{
    "break.c",
    "ellipsize.c",
    "fonts.c",
    "glyphstring.c",
    "itemize.c",
    "modules.c",
    "pango-attributes.c",
    "pango-bidi-type.c",
    "pango-color.c",
    "pango-context.c",
    "pango-coverage.c",
    "pango-emoji.c",
    "pango-engine.c",
    "pango-fontmap.c",
    "pango-fontset.c",
    "pango-fontset-simple.c",
    "pango-glyph-item.c",
    "pango-gravity.c",
    "pango-item.c",
    "pango-language.c",
    "pango-layout.c",
    "pango-markup.c",
    "pango-matrix.c",
    "pango-renderer.c",
    "pango-script.c",
    "pango-tabs.c",
    "pango-utils.c",
    "reorder-items.c",
    "shape.c",
    "serializer.c",
    "json/gtkjsonparser.c",
    "json/gtkjsonprinter.c",

    "pangowin32.c",
    "pangowin32-fontcache.c",
    "pangowin32-fontmap.c",
    "pangowin32-dwrite-fontmap.cpp",

    "pangocairo-context.c",
    "pangocairo-font.c",
    "pangocairo-fontmap.c",
    "pangocairo-render.c",
    "pangocairo-fcfont.c",
    "pangocairo-fcfontmap.c",
    "pangocairo-win32font.c",
    "pangocairo-win32fontmap.c",
};

const CAIRO_SRCS = [_][]const u8{
    "cairo-analysis-surface.c",
    "cairo-arc.c",
    "cairo-array.c",
    "cairo-atomic.c",
    "cairo-base64-stream.c",
    "cairo-base85-stream.c",
    "cairo-bentley-ottmann-rectangular.c",
    "cairo-bentley-ottmann-rectilinear.c",
    "cairo-bentley-ottmann.c",
    "cairo-botor-scan-converter.c",
    "cairo-boxes-intersect.c",
    "cairo-boxes.c",
    "cairo-cache.c",
    "cairo-clip-boxes.c",
    "cairo-clip-polygon.c",
    "cairo-clip-region.c",
    "cairo-clip-surface.c",
    "cairo-clip-tor-scan-converter.c",
    "cairo-clip.c",
    "cairo-color.c",
    "cairo-composite-rectangles.c",
    "cairo-compositor.c",
    "cairo-contour.c",
    "cairo-damage.c",
    "cairo-debug.c",
    "cairo-default-context.c",
    "cairo-device.c",
    "cairo-error.c",
    "cairo-fallback-compositor.c",
    "cairo-fixed.c",
    "cairo-font-face-twin-data.c",
    "cairo-font-face-twin.c",
    "cairo-font-face.c",
    "cairo-font-options.c",
    "cairo-freed-pool.c",
    "cairo-freelist.c",
    "cairo-gstate.c",
    "cairo-hash.c",
    "cairo-hull.c",
    "cairo-image-compositor.c",
    "cairo-image-info.c",
    "cairo-image-source.c",
    "cairo-image-surface.c",
    "cairo-line.c",
    "cairo-lzw.c",
    "cairo-mask-compositor.c",
    "cairo-matrix.c",
    "cairo-mempool.c",
    "cairo-mesh-pattern-rasterizer.c",
    "cairo-misc.c",
    "cairo-mono-scan-converter.c",
    "cairo-mutex.c",
    "cairo-no-compositor.c",
    "cairo-observer.c",
    "cairo-output-stream.c",
    "cairo-paginated-surface.c",
    "cairo-path-bounds.c",
    "cairo-path-fill.c",
    "cairo-path-fixed.c",
    "cairo-path-in-fill.c",
    "cairo-path-stroke-boxes.c",
    "cairo-path-stroke-polygon.c",
    "cairo-path-stroke-traps.c",
    "cairo-path-stroke-tristrip.c",
    "cairo-path-stroke.c",
    "cairo-path.c",
    "cairo-pattern.c",
    "cairo-pen.c",
    "cairo-polygon-intersect.c",
    "cairo-polygon-reduce.c",
    "cairo-polygon.c",
    "cairo-raster-source-pattern.c",
    "cairo-recording-surface.c",
    "cairo-rectangle.c",
    "cairo-rectangular-scan-converter.c",
    "cairo-region.c",
    "cairo-rtree.c",
    "cairo-scaled-font.c",
    "cairo-shape-mask-compositor.c",
    "cairo-slope.c",
    "cairo-spans-compositor.c",
    "cairo-spans.c",
    "cairo-spline.c",
    "cairo-stroke-dash.c",
    "cairo-stroke-style.c",
    "cairo-surface-clipper.c",
    "cairo-surface-fallback.c",
    "cairo-surface-observer.c",
    "cairo-surface-offset.c",
    "cairo-surface-snapshot.c",
    "cairo-surface-subsurface.c",
    "cairo-surface-wrapper.c",
    "cairo-surface.c",
    "cairo-time.c",
    "cairo-tor-scan-converter.c",
    "cairo-tor22-scan-converter.c",
    "cairo-toy-font-face.c",
    "cairo-traps-compositor.c",
    "cairo-traps.c",
    "cairo-tristrip.c",
    "cairo-unicode.c",
    "cairo-user-font.c",
    "cairo-version.c",
    "cairo-wideint.c",
    "cairo.c",
    "cairo-cff-subset.c",
    "cairo-scaled-font-subsets.c",
    "cairo-truetype-subset.c",
    "cairo-type1-fallback.c",
    "cairo-type1-glyph-names.c",
    "cairo-type1-subset.c",
    "cairo-type3-glyph-surface.c",
    "cairo-pdf-operators.c",
    "cairo-pdf-shading.c",
    "cairo-tag-attributes.c",
    "cairo-tag-stack.c",
    "cairo-deflate-stream.c",
};

const PIXMAN_SRCS = [_][]const u8{
    "pixman.c",
    "pixman-access.c",
    "pixman-access-accessors.c",
    "pixman-bits-image.c",
    "pixman-combine32.c",
    "pixman-combine-float.c",
    "pixman-conical-gradient.c",
    "pixman-filter.c",
    "pixman-x86.c",
    "pixman-mips.c",
    "pixman-arm.c",
    "pixman-ppc.c",
    "pixman-edge.c",
    "pixman-edge-accessors.c",
    "pixman-fast-path.c",
    "pixman-glyph.c",
    "pixman-general.c",
    "pixman-gradient-walker.c",
    "pixman-image.c",
    "pixman-implementation.c",
    "pixman-linear-gradient.c",
    "pixman-matrix.c",
    "pixman-noop.c",
    "pixman-radial-gradient.c",
    "pixman-region16.c",
    "pixman-region32.c",
    "pixman-solid-fill.c",
    "pixman-timer.c",
    "pixman-trap.c",
    "pixman-utils.c",
};
