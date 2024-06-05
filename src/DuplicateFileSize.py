import os
def duplicate_file():
    with open("data/data.csv", "r") as file:
        # Duplicate the file
        with open("data/data_copy.csv", "w") as copy:
            read = file.read()
            copy.write(read)
            if read[-1] != "\n":
                copy.write("\n")
            copy.write(read)
            if read[-1] != "\n":
                copy.write("\n")
            copy.write(read)
            if read[-1] != "\n":
                copy.write("\n")
            copy.write(read)
    print("File duplicated successfully")
    os.remove("data/data.csv")
    os.rename("data/data_copy.csv", "data/data.csv")

if __name__ == "__main__":
    duplicate_file()