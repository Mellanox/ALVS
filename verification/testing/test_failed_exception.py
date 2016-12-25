class TestFailedException(Exception):
	def __init__(self, error_message):

		super(TestFailedException, self).__init__(error_message)