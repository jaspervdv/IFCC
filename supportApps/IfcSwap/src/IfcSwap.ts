import path from "path";
import fs from "fs";
import * as FRAGS from "@thatopen/fragments";

// Read IFC file into a Uint8Array

if (process.argv.length < 4 )
{
    console.error("Requires an input and output path");
    process.exit(0);
}
if (process.argv.length > 4 )
{
    console.error("Too many input args");
    process.exit(0);
}

process.argv.length

const filePathIn = process.argv[2];
const filepathOut = process.argv[3];

console.log("Input path: " + filePathIn);
console.log("Output path: " + filepathOut);

const ifcBytes = new Uint8Array(fs.readFileSync(filePathIn));
const importer = new FRAGS.IfcImporter();

importer.wasm = {
  path: path.join(path.dirname(process.execPath), "\\"),
  absolute: true
};

async function convert() {
    importer.addAllAttributes();
    importer.addAllRelations();

    const fragBytes = await importer.process({
        bytes: ifcBytes,
        raw: false
    });

    fs.writeFileSync(filepathOut, fragBytes);

    console.log("✅ IFC converted to model.frag");
}

convert().catch(console.error);