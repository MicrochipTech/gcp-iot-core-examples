import os
import argparse
import json
import datetime
from google.cloud import pubsub_v1
import tkinter as tk
import tkinter.ttk as ttk


class GcpGui(tk.Frame):
    """Basic Message Visualizer gui"""
    def __init__(self, project_id, subscription_id, master=None):
        tk.Frame.__init__(self, master)
        self.pack()

        info_frame = tk.Frame(master)
        info_frame.pack(fill='x', side='top')
        info_frame.columnconfigure(1, weight=1)

        tk.Label(info_frame, text='Project').grid(row=0)
        tk.Label(info_frame, text='Registry').grid(row=1)
        tk.Label(info_frame, text='Region').grid(row=2)

        self.project = tk.StringVar()
        project = tk.Entry(info_frame, width=50, textvariable=self.project)
        project.grid(row=0, column=1, sticky=tk.E+tk.W)

        self.registry = tk.StringVar()
        registry = tk.Entry(info_frame, width=50, textvariable=self.registry)
        registry.grid(row=1, column=1, sticky=tk.E+tk.W)

        self.region = tk.StringVar()
        region = tk.Entry(info_frame, width=50, textvariable=self.region)
        region.grid(row=2, column=1, sticky=tk.E+tk.W)

        data_frame = tk.Frame(master)
        data_frame.pack(fill='both', side='bottom', expand=1)

        self.tree = ttk.Treeview(data_frame, selectmode='browse')
        self.tree.pack(side='left', fill='both', expand=1)

        vsb = ttk.Scrollbar(data_frame, orient='vertical', command=self.tree.yview)
        vsb.pack(side='right', fill='y')
        self.tree.configure(yscrollcommand=vsb.set)

        self.subscriber = pubsub_v1.SubscriberClient()
        self.subscription_path = self.subscriber.subscription_path(project_id, subscription_id)
        self.subscriber.subscribe(self.subscription_path, callback=self.subscription_callback)

    def subscription_callback(self, message):
        """Receive messages from the subscription"""
        data = json.loads(message.data)

        self.project.set(message.attributes['projectId'])
        self.registry.set(message.attributes['deviceRegistryId'])
        self.region.set(message.attributes['deviceRegistryLocation'])

        sample_values = [message.attributes['deviceId']] + \
                        ['{}: {}'.format(k, v) for k, v in data.items() if k != 'timestamp']
        sample_time = datetime.datetime.fromtimestamp(data['timestamp'])

        self.tree.configure(columns=['' for _ in range(len(sample_values))])
        self.tree.insert('', 'end', text=sample_time, values=sample_values)
        self.tree.yview_moveto(1)

        message.ack()


def gcp_gui(project_id, subscription_id):
    """Create a window with the visualizer gui"""
    root = tk.Tk()
    root.wm_title('Microchip GCP Example')
    root.iconbitmap('shield.ico')
    app = GcpGui(project_id, subscription_id, master=root)
    app.mainloop()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='GCP Example Gui')
    parser.add_argument('subscription', help='Topic Subscription')
    parser.add_argument('--creds', help='Credential Json File')
    args = parser.parse_args()

    if args.creds is not None:
        os.environ["GOOGLE_APPLICATION_CREDENTIALS"] = args.creds

    with open(os.environ["GOOGLE_APPLICATION_CREDENTIALS"]) as f:
        credentials = json.load(f)
        project = credentials['project_id']

    gcp_gui(project, args.subscription)
