import com.android.zipflinger.Sources;
import com.android.zipflinger.ZipArchive;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.zip.Deflater;

/**
 * Build a single-entry jar the same way AGP compress*Assets does, but stream
 * large files via zipflinger Sources.from / LargeFileSource instead of BytesSource
 * (which OOMs / "Required array size too large" above ~2GiB).
 *
 * Usage: java -cp zipflinger.jar MakeStoredAssetJar <inputFile> <outputJar> <entryName>
 */
public class MakeStoredAssetJar {
    public static void main(String[] args) throws Exception {
        if (args.length != 3) {
            System.err.println("Usage: MakeStoredAssetJar <inputFile> <outputJar> <entryName>");
            System.exit(2);
        }
        Path input = Path.of(args[0]);
        Path output = Path.of(args[1]);
        String entryName = args[2];
        if (!Files.isRegularFile(input)) {
            System.err.println("Input not found: " + input);
            System.exit(1);
        }
        Files.createDirectories(output.getParent());
        Files.deleteIfExists(output);
        long size = Files.size(input);
        System.out.println("Writing STORED entry '" + entryName + "' (" + size + " bytes) -> " + output);
        try (ZipArchive archive = new ZipArchive(output)) {
            archive.add(Sources.from(input, entryName, Deflater.NO_COMPRESSION));
        }
        System.out.println("Wrote " + Files.size(output) + " byte jar");
    }
}
