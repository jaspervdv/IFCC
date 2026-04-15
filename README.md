# IFCC

<!-- markdownlint-disable MD033 -->
<!-- markdownlint-disable MD034 -->

IFCC is a console application that almost lossless compresses IFC and IFCZIP files. The tool allows both IFC and IFCZIP output. IFCZIP allows for more compression but will use the IFCZIP encoding which might not be supported by all applications. The general processes of the compressor are:

* Rounding of floating number values to 0.000001 meter if their precision is higher (0.000001 meter is still smaller than the width of a human hair).
* Eliminating redundant data by merging objects that encode identical data. Objects such as IfcCartesianPoint, IfcDirection, and IfcPropertySingleValue. Any object that has a GUID is not touched. This sub-process is based on the method developed by [(Sun et al., 2016)](#1) but rewritten from scratch
* Eliminating data that is not (directly or indirectly) related or relating to the main IfcProject object.
* Restructuring the order in which objects are stored so that the most referenced objects are placed at the beginning of the file. This reduces potential bloating due to large IDs being repeated often in referenced lists of other objects.
* Recalculating object IDs so that the size of both the class IDs and their references are kept at a minimum.
* Stripping line breaks and whitespaces.

![overview](./Images/overview.jpg "An example of the difference between a compressed and uncompressed IFC file")

A visual example of the redundancy removal methodology can be seen below. This model (the [FZK Haus](https://www.ifcwiki.org/index.php?title=KIT_IFC_Examples)) can be considered a representation of a normal IFC file for this case. The model uses multiple IfcQuantityArea objects to encode exactly the same data but for different windows: Area = 2.4 m<sup>2</sup>. All but a single object that encodes this data can be removed without losing data. Even in a simple file thousands of these situations occur which can be collapsed into a small pool of single unique objects. For this particular IFC model 9.830 different data objects can be eliminated.

![uncompressed relationships](./Images/unCompressedQuan.jpg "An example of uncompressed relationships")

![Compressed relationships](./Images/CompressedQuan.jpg "An example of compressed relationships")

| IFC file name | File size (MB) | IFCC compressed IFC file size (MB) | IFCC compressed IFCZIP file size (MB)* | IFCC compressed Fragments file size (MB)** |
| - | - | - | - | - |
| [FZK Haus](https://www.ifcwiki.org/index.php?title=KIT_IFC_Examples) | 2.511 | 1.615 (63.3%) | 0.307 (12.2%) | 0.213 (8.5%) |
| [Office Building](https://www.ifcwiki.org/index.php?title=KIT_IFC_Examples) | 10.679 | 2.363 (22.1%) | 0.539 (5.0%) | 0.808 (7.6%) |
| [Smiley West](https://www.ifcwiki.org/index.php?title=KIT_IFC_Examples) | 5.967 | 2.178 (36.5%) | 0.521 (8.7%) | 0.625 (10.5%) |
| [Schependomlaan](https://github.com/jakob-beetz/DataSetSchependomlaan/tree/master) | 63.554 | 18.004 (28.3%) | 3.199 (5.0%) | 2.635 (4.1%) |
| [Strijp S architectural - BIM bouwkundig](https://github.com/buildingsmart-community/Community-Sample-Test-Files/tree/main/IFC%202.3.0.1%20(IFC%202x3)/SDK%20-%20S1) | 333.941 | 58.661 (17.6%) | 13.218 (4.0%) | 14.760 (4.4%) |
| | | | | |
| Average size | 100% | 33.6% | 7.0% | 7.0% |

*The IFCZIP files are IFCC compressed IFC files and stored in the IFCZIP encoding by letting IFCC access 7Zip.

** The Fragments files are IFCC compressed IFC files stored in the Fragments encoding.

Compression numbers of some publicly available datasets can be seen in the table above. A reduction of 30% is already a decent score considering how simple the logic of this application is. However, more compression can be achieved even without zipping.

Even the size of Strijp S model of 334MB drops well below the 100MB threshold for the online use of [IfcGref](https://ifcgref.bk.tudelft.nl/). This allows user to properly georeference a model without having to build a local build of IfcGref.

## How to use

The pre-compiled windows executable can be downloaded from the [releases page](https://github.com/jaspervdv/IFCC/releases). The executable works as a console application. The minimal input required to run the application is an input path. The input path can belong to an IFC or an IFCZIP file. If only an input path is supplied the tool will run a full compression and store the compressed file at the folder of the input path with the filename being the same except for the addition of "_compressed". This behavior is also shown if an IFC or IFCZIP file is dragged onto the .exe in the folder explorer.

Optionally an output path can be supplied as a second path. If an output path is supplied the compressed file will be stored at this location. And output path can be .ifc or .ifczip depending on if normal IFC output or if further compression to ZIP is desired. If the output file path ends with .ifc a compressed IFC file will be created. If the output file path ends with .ifczip a compressed IFCZIP file will be created. This is not case sensitive.

The tool also exposes other settings:

* Decimal size of the float values in the file:
  * "--deciN"
  * N is the desired decimal size in the file. E.g. 4 => 0.0001, 6 => 0.000001. The smaller the decimal size the more compression can be achieved at the cost of reduced accuracy
  * Default value = 6
* Max iteration number:
  * "--imaxN"
  * N is the desired max iteration cycle that the tool is allowed to run. The lower this number the faster the processing at the cost of reduced compression
  * Default value = ∞
* Keep the line breaks in the file (pretty print):
  * "--prty"

Unlike earlier developed IFC related projects by me (such as the [IfcEnvelopeExtractor](https://github.com/tudelft3d/IFC_BuildingEnvExtractor)) the single IFCC executable can process all IFC versions.

If fragments export is desired IFCC.exe has to be placed in the same folder as IfcSwap.exe and web-ifc-node.wasm. Sadly due to the libraries used no other reasonable solution could be found.

If maximal IFCZIP compression is desired it is recommended to have 7zip installed on the system the code is ran on. If 7zip is present the code will use that for the zip compression. If it is unable to find 7zip minizip-ng is used for the compression.

## How to build

The code is supplied with a CMakeLists.txt file that will handle all the C++ related downloads and dependencies. Cmake can behave slightly feeble and manual configuration might be required.

The only 3rd party library IFCC relies on is the [minizip-ng](https://github.com/zlib-ng/minizip-ng) library.

Fragments output is partially supported via IfcSwap. IfcSwap can be build by running the "BuildIfcSwap.bat" (for Windows) or "BuildIfcSwap.bat" (for Linux) file. [npm](https://www.npmjs.com/) is required for these files to run. IFCC has to have IfcSwap.exe and web-ifc-node.wasm in the same folder to be able to export Fragments files. This pairing can be set automatically during IFCC building by setting -DLINK_IFCSWAP=ON when running cmake.

However, the application also functions without IfcSwap, In that case fragments output will not be possible.

## Future development

Currently the application always does a full compression. In the future options will be added to exclude and fine tune certain processes.

At this moment IFCC processes the file as a (almost) flat text file. It would be interesting to see how much extra compression could be achieved with more complex comparisons. For example, similar identically shaped geometric objects could be stored as single objects with different transformations. This is a complex task and might slow the tool down significantly, so its usefulness has to evaluated.

## FAQ

**What is the difference between IFCC processed IFC files and normal IFCZIP files?**

IFCC achieves compression by restructuring an IFC file and removing redundant data. Zipping an IFC file reduces the size of an IFC file by changing the encoding. These zipped IFCZIP files do however still contain the redundant data of the unzipped IFC files. Although this is the case the compression achieved by zipping creates IFCZIP files that are usually smaller than IFCC processed unzipped IFC files.

IFCC does also support IFCZIP file output. This combines the processes of IFCC and ZIP for even further file size reductions. An IFCC created IFCZIP file will occupy noticeably less memory than a "normal" IFCZIP files. Some examples can be seen below.

| IFC file name | File size (MB) | IFCZIP file size (MB) | IFCC compressed IFCZIP file size (MB) |
| - | - | - | - |
| [FZK Haus](https://www.ifcwiki.org/index.php?title=KIT_IFC_Examples) | 2.511 | 0.415 (16.5%) | 0.307 (12.2%) |
| [Office Building](https://www.ifcwiki.org/index.php?title=KIT_IFC_Examples) | 10.679 | 1.469 (13.8%) | 0.539 (5.0%) |
| [Smiley West](https://www.ifcwiki.org/index.php?title=KIT_IFC_Examples) | 5.967 | 1.097 (18.4%) | 0.521 (8.7%) |
| [Schependomlaan](https://github.com/jakob-beetz/DataSetSchependomlaan/tree/master) | 63.554 | 8.048 (12.7%) | 3.199 (5.0%) |
| [Strijp S architectural - BIM bouwkundig](https://github.com/buildingsmart-community/Community-Sample-Test-Files/tree/main/IFC%202.3.0.1%20(IFC%202x3)/SDK%20-%20S1) | 333.941 | 52.883 (15.8%) | 13.218 (4.0%) |

**The application I want to open an IFCZIP file in does not support IFCZIP, what now?**

IFCZIP is just a ZIP archive but with a custom .IFCZIP extension. Changing the extension name from .IFCZIP to .ZIP or opening the .IFCZIP file with a file archiver (such as 7ZIP, WinRaR, or WinZip) will allow you to access the unzipped IFC file.

**What is the difference between IFCC processed IFC files and normal Fragments files?**

IFCC achieves compression by restructuring an IFC file and removing redundant data. Converting an IFC file to Fragments reduces the size of an IFC file by changing the encoding. These converted Fragments files do however still contain some of the redundant data of the original IFC files. Although this is the case the compression achieved by the conversion still causes Fragments files to be smaller than IFCC processed unzipped IFC files.

IFCC does also partially support Fragment file output. This combines the advantages of both IFCC and Fragments for even further file size reductions. An IFCC created Fragments file will occupy noticeably less memory than a "normal" Fragments files. Some examples can be seen below.

| IFC file name | File size (MB) | Fragments file size (MB) | IFCC compressed Fragments file size (MB) |
| - | - | - | - |
| [FZK Haus](https://www.ifcwiki.org/index.php?title=KIT_IFC_Examples) | 2.511 | 0.294 (11.7%) | 0.213 (8.5%) |
| [Office Building](https://www.ifcwiki.org/index.php?title=KIT_IFC_Examples) | 10.679 | 1.891 (17.7%) | 0.808 (7.6%) |
| [Smiley West](https://www.ifcwiki.org/index.php?title=KIT_IFC_Examples) | 5.967 | 0.854 (14.3%) | 0.625 (10.5%) |
| [Schependomlaan](https://github.com/jakob-beetz/DataSetSchependomlaan/tree/master) | 63.554 | 5.674 (8.9%) | 2.635 (4.1%) |
| [Strijp S architectural - BIM bouwkundig](https://github.com/buildingsmart-community/Community-Sample-Test-Files/tree/main/IFC%202.3.0.1%20(IFC%202x3)/SDK%20-%20S1) | 333.941 | 51.438 (15.4%) | 14.760 (4.4%) |

**Does IFCC support Fragments I/O?**

Only partial Fragments output is supported. Fragments is primarily used effective visual rendering, and so not all the data is transferred in a reversible manner. This means that it does not fit directly in the IFCC philosophy. However, Fragments was requested so often that it has been added if visualization is the main goal.

If normal IFC use is desired it is recommended to store the file as an IFC or IFCZIP file.

**Is compression done with IFCC lossless?**

Almost, but the tool steps trough a couple of processes:

* Rounding of the float numbers: Lossy
* Eliminating redundant data: Lossless in most cases*
* Eliminating unrelated or unrelating data: Lossy*
* Restructuring of the file: Lossless
* Recalculating object IDs: Lossless
* Stripping line breaks and whitespaces: Lossless

When storing to IFCZIP:

* Zipping: LossLess

When storing to Fragments:

* File conversion: Lossy

*IFC contains many repeating data objects that can usually be eliminated without the effective loss of data. This process can, in theory, be reversed. However, if objects were linked via their object relationships, so that if one is updated a group of linked objects is also updated with the same change, IFCC compression will break that. This way of linking is however something that is not often done in practice, and I also never encountered aside from theories.

**Dangling data, or data that is not related or relating to the file's IfcProject object is often unused data. This data can be removed in most cases. However, this deletion cannot be reversed. So, if there was a purpose to add dangling data it can result in loss of data

**Can IfcSwap be used as a standalone application?**

Yes, IfcSwap can be used to convert IFC files to Fragments without any extra compression operations. IfcSwap takes an input path, and output path and an optional compression argument. If as final argument "--c" is given it will compress according to the default settings set by Frags importer. If this argument is not given the Frags importer will be set to preserve all attributes and relations of the input file.

**Why does IFCC search for 7-Zip?**

Because IFCC uses [7-Zip](https://7-zip.org/) for zip compression to create IFCZIP files. IFCZIP files created by 7-Zip are significantly smaller than I was able to produce with [minizip-ng](https://github.com/zlib-ng/minizip-ng). If however 7-Zip is not found by the tool it defaults back to zipping using [minizip-ng](https://github.com/zlib-ng/minizip-ng) so using installing 7-Zip is not forced upon you.

## References

<a id="1"></a>
Sun, Jing, Yu-Shen Liu, Ge Gao, and Xiao-Guang Han. "IFCCompressor: A content-based compression algorithm for optimizing Industry Foundation Classes files." Automation in Construction 50 (2015): 1-15.
