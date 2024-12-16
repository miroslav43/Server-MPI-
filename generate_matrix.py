import random

def generate_matrix(filename, N, min_val=0.0, max_val=10.0):
    with open(filename, "w") as f:
        for i in range(N):
            row = [str(round(random.uniform(min_val, max_val), 2)) for _ in range(N)]
            f.write(" ".join(row) + "\n")

if __name__ == "__main__":
    # Matrices used in commands:
    # A.txt, B.txt with size 4
    generate_matrix("input/A.txt", 4)
    generate_matrix("input/B.txt", 4)

    # medA.txt, medB.txt with sizes 256 and 300 (based on commands)
    generate_matrix("input/medA.txt", 256)
    generate_matrix("input/medB.txt", 256)  # For MATRIXADD 256 medA.txt medB.txt

    generate_matrix("input/medA_300.txt", 300)
    generate_matrix("input/medB_300.txt", 300)  
    # Note: You referenced `medA.txt`, `medB.txt` for both 256 and 300 sized matrices.  
    # To avoid confusion, let's use `medA.txt` and `medB.txt` for 256 and `medA_300.txt` and `medB_300.txt` for 300.  
    # Update your command file accordingly if needed.

    # bigA.txt, bigB.txt for 1024 and 512 operations
    generate_matrix("input/bigA.txt", 1024)
    generate_matrix("input/bigB.txt", 1024)
    # This also covers the 512-size operation, as you can just use the same bigA.txt/bigB.txt  
    # or create separate files for 512. If you prefer separate files:
    generate_matrix("input/bigA_512.txt", 512)
    generate_matrix("input/bigB_512.txt", 512)

    # hugeA.txt, hugeB.txt for 2048 operations
    generate_matrix("input/hugeA.txt", 2048)
    generate_matrix("input/hugeB.txt", 2048)

    print("Matrix files generated successfully!")