import os
import sys

def rename_wav_files(directory_path):
    try:
        for i, filename in enumerate(os.listdir(directory_path)):
            if filename.endswith('.wav'):
                old_path = os.path.join(directory_path, filename)

                # should be in format 000 something.wav                

                old_name = ' '.join(old_path.split(' ')[2:])
                new_filename = "%03i %s" % (i, old_name)

                # new_filename = filename[:-4]  # Remove the '.wav' extension
                new_path = os.path.join(directory_path, new_filename)  # Replace 'new_extension' with your desired new extension
                os.rename(old_path, new_path)
                print(f"Renamed {old_path} to {new_path}")
    except Exception as e:
        print(f"An error occurred: {str(e)}")

if __name__ == "__main__":
    # Replace 'directory_path' with the path to the directory containing the .wav files
    directory_path = sys.argv[1]
    rename_wav_files(directory_path)