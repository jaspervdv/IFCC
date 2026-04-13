import path from "path";
import fs from "fs";
import * as FRAGS from "@thatopen/fragments";

function getFileExtension(filename: string): string {
    return filename.slice(filename.lastIndexOf('.') + 1);
}

async function convert(
    importer: FRAGS.IfcImporter, 
    ifcBytes : Uint8Array<ArrayBuffer>,
    Uint8Array : string
) {
    const fragBytes = await importer.process({
        bytes: ifcBytes,
        raw: false
    });

    fs.writeFileSync(filepathOut, fragBytes);

    console.log("✅ IFC converted to model.frag");
}

// Read IFC file into a Uint8Array

if (process.argv.length < 4 )
{
    console.error("Requires an input and output path");
    process.exit(0);
}
if (process.argv.length > 5 )
{
    console.error("Too many input args");
    process.exit(0);
}

const filePathIn = process.argv[2];
const filepathOut = process.argv[3];

// check if all attributes and relations are required (default is yes)
let isFullRes = true;
if (process.argv.length == 5){
    if(process.argv[4].toUpperCase() == "--C"){
        isFullRes = false;
    }
}

console.log("Input path: " + filePathIn);
console.log("Output path: " + filepathOut);

if (!fs.existsSync(filePathIn))
{
    console.error("Input path does not exist");
    process.exit(0);
}

if (getFileExtension(filePathIn).toUpperCase() != "IFC")
{
    console.error("Input path is not IFC");
    process.exit(0);
}

if (getFileExtension(filepathOut).toUpperCase() != "FRAG")
{
    console.error("Output path is not FRAG");
    process.exit(0);
}

const ifcBytes = new Uint8Array(fs.readFileSync(filePathIn));
const importer = new FRAGS.IfcImporter();

if (process.platform  == "win32") 
{ 
    importer.wasm = {
    path: path.join(path.dirname(process.execPath), "\\"),
    absolute: true
    };
}
else
{
    importer.wasm = {
    path: path.join(path.dirname(process.execPath), "/"),
    absolute: true
    };
}

if (isFullRes){
    importer.addAllAttributes();
    importer.addAllRelations();
}

convert(importer, ifcBytes, filepathOut).catch(console.error);