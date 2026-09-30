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
    <div className="scrim">
      <form
        className="dialog"
        onSubmit={(event) => {
          event.preventDefault();
          onSubmit(values);
        }}
      >
        <h2 className="room-title">{title}</h2>
        <div className="dialog-stack">
          {fields.map((field) => (
            <label key={field.name} className="field-label">
              <span>{field.label}</span>
              {field.options ? (
                <>
                  <input
                    className="field"
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
                  className="field"
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
        <div className="dialog-actions">
          <button type="button" className="ghost" onClick={onCancel}>
            Cancel
          </button>
          <button type="submit" className="primary">
            OK
          </button>
        </div>
      </form>
    </div>
  );
}
