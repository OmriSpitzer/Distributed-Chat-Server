import { useState } from "react";

export default function FormDialog({ title, fields, onCancel, onSubmit }) {
  const [values, setValues] = useState(() => {
    const initial = {};
    for (const field of fields) {
      initial[field.name] = field.value ?? field.options?.[0] ?? "";
    }
    return initial;
  });

  return (
    <div className="fixed inset-0 z-10 flex items-center justify-center bg-stone-900/40 p-4">
      <form
        className="w-full max-w-sm rounded-2xl bg-white p-6"
        onSubmit={(event) => {
          event.preventDefault();
          onSubmit(values);
        }}
      >
        <h2 className="text-lg font-semibold">{title}</h2>
        <div className="mt-4 space-y-3">
          {fields.map((field) => (
            <label key={field.name} className="block text-sm">
              <span className="mb-1 block text-stone-600">{field.label}</span>
              {field.options ? (
                <>
                  <input
                    className="w-full rounded-lg border border-stone-300 px-3 py-2"
                    list={`${field.name}-options`}
                    value={values[field.name] ?? ""}
                    onChange={(event) => setValues({ ...values, [field.name]: event.target.value })}
                  />
                  <datalist id={`${field.name}-options`}>
                    {field.options.map((option) => (
                      <option key={option} value={option} />
                    ))}
                  </datalist>
                </>
              ) : (
                <input
                  className="w-full rounded-lg border border-stone-300 px-3 py-2"
                  type={field.type ?? "text"}
                  readOnly={field.readOnly}
                  placeholder={field.placeholder}
                  value={values[field.name] ?? ""}
                  onChange={(event) => setValues({ ...values, [field.name]: event.target.value })}
                />
              )}
            </label>
          ))}
        </div>
        <div className="mt-5 flex justify-end gap-2">
          <button type="button" className="rounded-lg border border-stone-300 px-3 py-2 text-sm" onClick={onCancel}>
            Cancel
          </button>
          <button type="submit" className="rounded-lg bg-stone-900 px-3 py-2 text-sm text-white">
            OK
          </button>
        </div>
      </form>
    </div>
  );
}
