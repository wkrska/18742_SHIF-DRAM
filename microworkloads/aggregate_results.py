def read_line(filename):
  """Reads and returns the 13th line of a file, handling errors.

  Args:
    filename: The name of the file to read.

  Returns:
    The 13th line of the file, or None if an error occurs.
  """
  try:
    with open(filename, 'r') as f:
      for _ in range(13):  # Skip the first N lines
        f.readline()
      return f.readline().rstrip('\n')  # Read and remove trailing newline
  except IOError:
    print("Error opening file:", filename)
    return None

if __name__ == '__main__':
  S = [ 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536];
  D = [ 8, 16, 32];

  # Loop through files with increasing numbers
  for s in S:
    for d in D:
      filename = "temp_results/" + str(s) + "_" + str(d) + ".txt"
      line = read_line(filename)
      print(line)