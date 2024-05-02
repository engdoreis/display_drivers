// Copyright (c) 2024 Gary Guo.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

use std::path::PathBuf;

use anyhow::{ensure, Result};
use clap::Parser;

#[derive(Parser)]
struct Args {
    font: PathBuf,

    #[arg(long, default_value = "16")]
    font_size: f32,

    #[arg(long, default_value = "")]
    font_name: String,
}

fn main() -> Result<()> {
    let args = Args::parse();

    let font = std::fs::read(&args.font)?;
    let font = fontdue::Font::from_bytes(font, fontdue::FontSettings::default())
        .map_err(|err| anyhow::anyhow!("{}", err))?;

    // First rasterize all printable ASCII characters.
    let mut rasterization = Vec::new();
    for c in ' '..='~' {
        let (metrics, bitmap) = font.rasterize(c, args.font_size);
        ensure!(
            metrics.xmin >= 0,
            "fonts that render at negative x offset is not supported"
        );
        rasterization.push((metrics, bitmap));
    }

    let max_y = rasterization
        .iter()
        .map(|(metrics, _)| metrics.ymin + metrics.height as i32)
        .max()
        .unwrap();
    let min_y = rasterization
        .iter()
        .map(|(metrics, _)| metrics.ymin)
        .min()
        .unwrap();

    let max_width = rasterization
        .iter()
        .map(|(metrics, _)| metrics.width)
        .max()
        .unwrap();
    let max_height = (max_y - min_y) as usize;

    ensure!(
        max_width <= 8,
        "this tool cannot generate font with width greater than 8 pixels"
    );

    println!("#include <font.h>");

    println!(
        "// Character bitmaps for {} {}pt",
        args.font_name, args.font_size
    );
    println!(
        "const unsigned char {}_{}ptBitmaps[] = {{",
        heck::AsLowerCamelCase(&args.font_name),
        args.font_size
    );

    for (i, c) in (' '..='~').enumerate() {
        if i != 0 {
            println!();
        }

        let (metrics, bitmap) = &rasterization[i];
        println!(
            "    // @{} '{}' ({} pixels wide)",
            c as usize, c, metrics.advance_width as usize
        );
        for y in (min_y..max_y).rev() {
            // The y axis goes bottom up but bitmap is top down.
            // Convert the index
            let y_offset = metrics.height as i32 - 1 - (y - metrics.ymin);
            if y_offset < 0 || y_offset >= metrics.height as i32 {
                println!("    0x00,  //");
                continue;
            }

            let y_offset = y_offset as usize;

            let mut row = 0u8;
            let mut pixel_art = String::new();
            for x in 0..metrics.width {
                let pixel = bitmap[y_offset as usize * metrics.width + x as usize];
                // dbg!(pixel);
                if pixel > 128 {
                    row |= 1 << x;
                    pixel_art.push('#');
                } else {
                    pixel_art.push(' ');
                }
            }

            println!("    0x{:02x},  // {}", row, pixel_art.trim_end());
        }
    }

    println!("}};");
    println!();
    println!(
        "// Character descriptors for {} {}pt",
        args.font_name, args.font_size
    );
    println!(
        "const FontCharInfo {}_{}ptDescriptors[] = {{",
        heck::AsLowerCamelCase(&args.font_name),
        args.font_size
    );
    for (i, c) in (' '..='~').enumerate() {
        let (metrics, _) = &rasterization[i];
        println!(
            "{}",
            format!(
                "    {{ {}, {} }},  // '{}'",
                metrics.advance_width as usize,
                max_height * i,
                c
            )
            .trim_end()
        );
    }

    println!(
        "}};

// Font information for {name} {size}pt
const Font {name_camel}_{size}ptFont = {{
    {height},                             //  Character height
    ' ',                            //  Start character
    '~',                            //  End character
    {name_camel}_{size}ptDescriptors,  //  Character descriptor array
    {name_camel}_{size}ptBitmaps,      //  Character bitmap array
}};",
        name = args.font_name,
        name_camel = heck::AsLowerCamelCase(&args.font_name),
        size = args.font_size,
        height = max_height,
    );

    Ok(())
}
