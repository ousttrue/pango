const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "pango",
        .target = target,
        .optimize = optimize,
    });
    exe.addCSourceFiles(.{
        .files = &.{"examples/cairoshape.c"},
    });
    b.installArtifact(exe);

    const fribidi = buildFribidi(b, target, optimize);
    const glib_dep = b.dependency("glib", .{
        .target = target,
        .optimize = optimize,
    });
    const glib = glib_dep.artifact("glib");
    const gobject = glib_dep.artifact("gobject");

    const pango = buildPango(b, target, optimize);
    pango.linkLibrary(fribidi);
    pango.linkLibrary(glib);
    pango.addIncludePath(glib.getEmittedIncludeTree().path(b, "glib"));
    pango.linkLibrary(gobject);

    exe.linkLibrary(pango);
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
    lib.addIncludePath(b.path("generated"));
    lib.addCSourceFiles(.{
        .root = b.path("pango"),
        .files = &PANGO_SRCS,
    });
    return lib;
}

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
    lib.installHeader(dep.path("lib/fribidi.h"), "fribidi.h");
    lib.installHeader(dep.path("lib/fribidi-common.h"), "fribidi-common.h");

    return lib;
}

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
};
